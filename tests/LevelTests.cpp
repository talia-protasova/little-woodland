#include "presentation/BoardPlayback.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace woodland;
void check(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
/**
 * Returns a deterministic stable/dead ordinary layout for targeted letter fixtures.
 * Every special placement or hole is introduced explicitly by its test, avoiding
 * random starting matches that would obscure the behavior being checked.
 */
Board::Tiles base() {
    Board::Tiles tiles;
    for (int r=0;r<7;++r) for(int c=0;c<7;++c) tiles[r*7+c]=static_cast<TileType>((r+c)%5);
    return tiles;
}
int letters(const Board& b) { return std::count(b.tiles().begin(),b.tiles().end(),TileType::Letter); }
/**
 * Searches by fully resolving swaps on copies until one succeeds. Unlike the
 * ordinary-only oracle helpers, this delegates letter restrictions to Board.
 * The original board/RNG is never advanced while selecting fixture input.
 */
std::pair<Cell,Cell> legal(const Board& board) {
    for(int r=0;r<7;++r) for(int c=0;c<7;++c) for(auto d:{Cell{1,0},Cell{0,1}}) {
        auto copy=board;
        if(copy.trySwap({r,c},{r+d.row,c+d.column}).valid) return {{r,c},{r+d.row,c+d.column}};
    }
    throw std::runtime_error("No legal move");
}
/**
 * Checks letters as nonmatchable, unswappable gravity survivors; exit-specific
 * delivery; stacked delivery traces; and letter anchoring through both shuffle and
 * regeneration recovery. Identical seeds compare traced and untraced execution.
 */
void rules() {
    auto tiles=base();
    tiles[0]=tiles[1]=tiles[2]=TileType::Letter;
    Board b(tiles);
    check(!b.hasMatches(),"Letters matched");
    Board::Trace trace;
    check(!b.trySwap({0,0},{1,0},&trace).valid && trace.empty(),"Letter directly swapped");
    // A berry could complete a run only by swapping with a letter. A naive legal-
    // swap detector would advertise that forbidden route and miss the dead board.
    auto blocked = tiles;
    blocked.fill(TileType::Letter);
    blocked[0] = blocked[2] = blocked[8] = TileType::Berry;
    Board onlyLetterSwap(blocked);
    check(!onlyLetterSwap.hasLegalSwap(), "Legal-swap detection proposed moving a letter");
    check(!onlyLetterSwap.trySwap({0,1},{1,1}).valid, "Letter completed an illegal ordinary match");
    auto prior=b.tiles(); b.removeMatches(); check(b.tiles()==prior,"Match removal removed letter");
    tiles=base();
    // Keep an acorn between two letters above a cleared column: all three must
    // retain their vertical order instead of treating letters as fixed obstacles.
    tiles[0]=TileType::Letter; tiles[7]=TileType::Acorn;
    tiles[14]=TileType::Letter;
    for(int r=3;r<7;++r)tiles[r*7]=TileType::Empty;
    Board gravity(tiles);
    std::vector<Board::Motion> motions; gravity.applyGravity(&motions);
    check(gravity.at(4,0)==TileType::Letter && gravity.at(5,0)==TileType::Acorn
          && gravity.at(6,0)==TileType::Letter,"Gravity reordered letters/survivors");
    gravity.refill(); check(letters(gravity)==2,"Refill generated letters");
    tiles=base(); tiles[42]=TileType::Letter;
    Board noExit(tiles,18); noExit.resolve();
    check(noExit.at(6,0)==TileType::Letter && noExit.delivered()==0,"Letter delivered without exit");
    // Stacked letters drain with a separate gravity/delivery transition for each.
    tiles[35]=TileType::Letter;
    Board exits(tiles,18,std::bitset<7>(1)), untraced=exits;
    exits.resolve(&trace); untraced.resolve();
    check(exits.delivered()==2 && letters(exits)==0,"Stacked letters not drained");
    check(exits.tiles()==untraced.tiles() && exits.delivered()==untraced.delivered(),"Recording changed delivery RNG");
    auto visible=tiles; int delivered=0;
    for(const auto& step:trace) {
        check(visible==step.before,"Delivery snapshots discontinuous");
        if(step.stage==Board::Stage::Delivery) {
            ++delivered;
            check(step.matched.count()==1 && step.before[42]==TileType::Letter
                  && step.after[42]==TileType::Empty,"Delivery stage incorrect");
        }
        visible=step.after;
    }
    check(delivered==2 && visible==exits.tiles(),"Delivery trace incomplete");
    // Both regular shuffles and fallback generation keep anchored letters.
    tiles=base();tiles[3]=TileType::Letter;
    Board dead(tiles,77), twin=dead;
    check(!dead.hasLegalSwap() && dead.recoverIfNoMoves(),"Dead board with letter not recovered");
    twin.recoverIfNoMoves();
    check(dead.at(0,3)==TileType::Letter && letters(dead)==1 && !dead.hasMatches()
          && dead.hasLegalSwap() && dead.tiles()==twin.tiles(),"Recovery moved/lost letter or diverged");
    // No ordinary type occurs three times in this multiset. Any permutation
    // therefore lacks a legal match-producing swap, forcing all shuffle attempts
    // to fail before regeneration can create new ordinary values. Seed 19 makes
    // that fallback reproducible; the fixture, not the seed, forces the fallback.
    tiles.fill(TileType::Letter);
    tiles[0]=TileType::Berry; tiles[1]=TileType::Acorn;
    tiles[2]=TileType::Star;
    tiles[7]=TileType::Leaf; tiles[8]=TileType::Mushroom; tiles[9]=TileType::Berry;
    Board fallback(tiles,19), fallbackTwin=fallback;
    check(fallback.recoverIfNoMoves(),"Fallback fixture did not recover");
    fallbackTwin.recoverIfNoMoves();
    check(fallback.hasLegalSwap() && !fallback.hasMatches() && letters(fallback)==43
          && fallback.tiles()==fallbackTwin.tiles(),"Fallback failed or diverged");
    for(int i=0;i<49;++i) if(tiles[i]==TileType::Letter)
        check(fallback.tiles()[i]==TileType::Letter,"Fallback moved letter anchor");
    for(unsigned seed=0;seed<40;++seed) {
        auto t=base();t[3]=TileType::Letter;
        Board recovery(t,seed); recovery.recoverIfNoMoves();
        check(recovery.at(0,3)==TileType::Letter && letters(recovery)==1,"Recovery relocated letter");
    }
}
/**
 * Replays the documented default-seed two-move solution through presentation,
 * checking delayed progress/outcome publication, input locks and trace determinism.
 * Repeating it after restart verifies that the restored RNG reproduces the win.
 */
void successfulDemo() {
    Level level(LevelDefinition::demo()), twin(LevelDefinition::demo());
    const auto original=level.board().tiles();
    BoardPlayback playback;
    // This exact zero-based path is the documented default-seed demo win. It is
    // replayed after restart to prove both solvability and restored random state.
    const std::pair<Cell,Cell> path[]={{{4,6},{5,6}},{{5,3},{6,3}}};
    int moves=0; bool cascade=false, delivery=false;
    for(auto pair:path) {
        Board::Trace trace, other;
        const auto result=level.trySwap(pair.first,pair.second,&trace);
        check(result.valid && twin.trySwap(pair.first,pair.second,&other).valid,"Demo solution rejected");
        cascade|=result.resolution.waves>1;
        check(level.state()==GameState::Playing && !level.acceptsInput(),"Outcome/input published before playback");
        check(trace.size()==other.size(),"Replay trace diverged");
        for(std::size_t i=0;i<trace.size();++i) {
            check(trace[i].stage==other[i].stage && trace[i].before==other[i].before
                  && trace[i].after==other[i].after && trace[i].matched==other[i].matched
                  && trace[i].motions.size()==other[i].motions.size(),"Replay snapshots diverged");
            delivery|=trace[i].stage==Board::Stage::Delivery;
        }
        auto committed=level.board().tiles();
        playback.play(trace);
        const int hidden=playback.pendingDeliveries();
        if(hidden)check(level.board().delivered()-hidden==0,"Progress exposed before delivery animation");
        playback.click(level,Cell{0,0});playback.click(level,Cell{0,1});
        check(level.board().tiles()==committed && !playback.selected(),"Input leaked during playback");
        playback.update(level,0.01);
        check(level.state()==GameState::Playing,"Early win/loss");
        playback.update(level,100);
        twin.finishTurn();
        ++moves;
        check(level.remainingMoves()==20-moves && level.board().tiles()==twin.board().tiles(),"Demo replay/budget diverged");
    }
    check(cascade && delivery && level.state()==GameState::Won && level.board().delivered()==1,"Demo did not win via cascades/delivery");
    const auto final=level.board().tiles();
    check(!level.trySwap({0,0},{0,1}).valid,"Won level accepted move");
    playback.click(level,Cell{0,0}); check(!playback.selected() && level.board().tiles()==final,"Won level accepted selection");
    level.restart(); playback=BoardPlayback{};
    check(level.board().tiles()==original && level.remainingMoves()==20 && level.board().delivered()==0
          && level.acceptsInput() && level.state()==GameState::Playing,"Restart failed");
    for(auto pair:path) { check(level.trySwap(pair.first,pair.second).valid,"Restart RNG diverged");level.finishTurn(); }
    check(level.board().tiles()==final && level.state()==GameState::Won,"Restart replay diverged");
    // A win on the very last move takes priority over defeat.
    auto d=LevelDefinition::demo(); d.moveLimit=2; Level last(d);
    for(auto pair:path){last.trySwap(pair.first,pair.second);last.finishTurn();}
    check(last.state()==GameState::Won && last.remainingMoves()==0,"Last-move win lost");
}
/**
 * Spends the demo budget using accepted nondelivery moves selected on copies.
 * This protects the real twenty-move boundary and confirms that a final loss, like
 * a win, rejects later input and can be restarted with the initial budget.
 */
void fullBudgetLoss() {
    Level level(LevelDefinition::demo());
    BoardPlayback playback;
    // Deliberately choose valid nondelivery moves so the real twenty-move budget
    // reaches defeat, rather than changing the game rules to manufacture a loss.
    for (int move = 0; move < 20; ++move) {
        bool found = false;
        for (int r = 0; r < 7 && !found; ++r) {
            for (int c = 0; c < 7 && !found; ++c) {
                for (auto delta : {Cell{0,1}, Cell{1,0}}) {
                    auto candidate = level;
                    Board::Trace trace;
                    if (!candidate.trySwap({r,c}, {r+delta.row,c+delta.column}, &trace).valid
                        || candidate.board().delivered() != 0) continue;
                    level = std::move(candidate);
                    playback.play(std::move(trace));
                    playback.update(level, 100);
                    found = true;
                    break;
                }
            }
        }
        check(found, "Full-budget loss fixture ran out of nondelivery moves");
        check(level.remainingMoves() == 19 - move, "Full-budget move count incorrect");
        check(level.state() == (move == 19 ? GameState::Lost : GameState::Playing),
              "Full-budget outcome occurred on wrong move");
    }
    check(!level.trySwap({0,0},{0,1}).valid, "Full-budget loss accepted gameplay");
    level.restart();
    check(level.remainingMoves() == 20 && level.acceptsInput(), "Full-budget restart failed");
}

/**
 * Separates rejected-attempt accounting from a real one-move defeat, then checks
 * restart during a pending turn. Invalid content variants isolate budget, target,
 * exit and duplicate-placement validation without changing production rules.
 */
void limitsAndValidation() {
    auto d=LevelDefinition::demo();d.moveLimit=1;
    Level level(d); BoardPlayback playback;
    const auto original=level.board().tiles();
    check(!level.trySwap({0,0},{0,2}).valid && level.remainingMoves()==1,"Invalid swap spent move");
    check(!level.trySwap({1,3},{1,4}).valid && level.remainingMoves()==1,"Letter swap spent move");
    Board::Trace trace;
    check(level.trySwap({4,6},{5,6},&trace).valid,"Loss fixture invalid");
    check(trace.back().stage==Board::Stage::Defeat && level.state()==GameState::Playing,"Loss not delayed");
    playback.play(trace);playback.update(level,100);
    check(level.state()==GameState::Lost && !level.acceptsInput() && level.remainingMoves()==0,"Move limit did not lose");
    check(!level.trySwap({0,0},{0,1}).valid,"Lost level accepted move");
    playback.click(level,Cell{0,0});check(!playback.selected(),"Lost level accepted selection");
    level.restart();check(level.board().tiles()==original && level.acceptsInput(),"Loss restart failed");
    // Restart also cancels an in-flight turn, without altering its future random stream.
    auto pair=legal(level.board());level.trySwap(pair.first,pair.second);level.restart();
    check(level.board().tiles()==original && level.remainingMoves()==1,"In-flight restart failed");
    // Isolate content-validation failures from turn mechanics: invalid budgets,
    // unreachable/missing exits and duplicate placements must fail at construction.
    for(int fixture=0;fixture<5;++fixture) {
        auto bad=d;
        if(fixture==0)bad.moveLimit=0;
        if(fixture==1)bad.requiredLetters=2;
        if(fixture==2)bad.exits.reset();
        if(fixture==3)bad.letters={{6,3}};
        if(fixture==4)bad.letters={{1,3},{1,3}};
        bool threw=false;try{Level invalid(bad);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"Invalid definition accepted");
    }
}
int main(){try{rules();successfulDemo();fullBudgetLoss();limitsAndValidation();std::cout<<"Letter rules, delivery traces, demo solution, budgets, outcomes and restart passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
