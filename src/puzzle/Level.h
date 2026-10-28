#pragma once
#include "Board.h"
#include <string>

namespace woodland {
/**
 * Describes one fixed-size level directly in C++; there is no level-file loader.
 *
 * Seed, budget, target and title have defaults. Callers must fully populate tiles
 * with ordinary pieces and supply letter cells and exit columns before creating
 * a Level; default construction does not generate the tile array. Letter positions
 * are overlaid separately so refill can keep the ordinary palette unchanged.
 * The current renderer uses fixed chapter text rather than a general content UI.
 */
struct LevelDefinition {
    std::string title = "WOODLAND POST";
    Board::Tiles tiles;
    std::uint32_t seed = 42397;
    int moveLimit = 20;
    int requiredLetters = 1;
    std::vector<Cell> letters;
    std::bitset<Board::size> exits;
    /**
     * Builds the demo definition from seeded ordinary tiles, one letter and its exit.
     * The default seed has a regression-tested two-move win; other seeds are exploratory.
     */
    static LevelDefinition demo(std::uint32_t seed = 42397);
};

/**
 * The level outcome, separate from transient swap/fall/refill presentation phases.
 * Playing also covers a pending resolved move whose trace has not yet finished.
 */
enum class GameState { Playing, Won, Lost };

/**
 * Owns validated level content, its Board, budget gate and final outcome.
 *
 * Board resolution commits immediately, but an accepted move stays pending until
 * finishTurn is called. BoardPlayback acknowledges the completed trace so cascades
 * and result transitions remain visible before another move can start. Restart
 * reconstructs the original seeded Board; presentation/audio reset is the caller's
 * responsibility. No OpenGL or audio resource belongs to this class.
 */
class Level {
public:
    /**
     * Copies/moves the definition into the level and constructs its initial Board.
     * @throws std::invalid_argument For invalid budget, target, initial cells or exits.
     * @throws std::runtime_error If required initial dead-board recovery cannot succeed.
     */
    explicit Level(LevelDefinition definition);
    const Board& board() const { return board_; }
    const LevelDefinition& definition() const { return definition_; }
    int remainingMoves() const { return definition_.moveLimit - static_cast<int>(board_.moves()); }
    GameState state() const { return state_; }
    /**
     * Allows moves only while Playing with no accepted move awaiting acknowledgement.
     * A zero-move pending turn remains locked until finishTurn publishes its outcome.
     */
    bool acceptsInput() const { return state_ == GameState::Playing && !pending_; }
    /**
     * Delegates an allowed attempt to Board and locks further moves on acceptance.
     * @param trace Optional Board transitions; a terminal UI stage is appended for
     * a completed objective or exhausted budget, after every Board animation step.
     * @return Rejection without mutation when the level is pending, won or lost.
     * Successful callers must eventually acknowledge the move with finishTurn.
     */
    SwapResult trySwap(Cell first, Cell second, Board::Trace* trace = nullptr);
    /**
     * Acknowledges a completed presentation trace and evaluates the pending outcome.
     * Winning takes priority when delivery and the last move happen together.
     * Does nothing if no accepted move is pending; it does not advance animation.
     */
    void finishTurn();
    /**
     * Rebuilds the starting Board and RNG from the stored definition before replacing
     * the current state. Counters, pending status and outcome reset together.
     * The application also replaces BoardPlayback and stops existing sound voices.
     */
    void restart();
private:
    /**
     * Validates content, overlays unique letters with same-column exits below them,
     * and recovers an initially dead board. Geometry/target checks establish a usable
     * configuration, not a general proof that every arbitrary seed is solvable.
     */
    static Board makeBoard(const LevelDefinition& definition);
    LevelDefinition definition_;
    Board board_;
    GameState state_ = GameState::Playing;
    bool pending_ = false;
};
}
