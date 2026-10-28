#include "presentation/BoardPlayback.h"
#include "rendering/BoardLayout.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace woodland;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

/**
 * Reuses the known demo path; a one-move budget yields defeat at its first step.
 * Checks outgoing fade, page visibility, final outcome publication and restart as
 * separate stages, so committed delivery counts cannot skip any Board animation.
 * @param won Selects the full winning path or its shortened-budget loss variant.
 */
void resultTiming(bool won) {
    auto definition = LevelDefinition::demo();
    if (!won) definition.moveLimit = 1;
    Level level(definition);
    const auto initial = level.board().tiles();
    BoardPlayback playback;
    check(!playback.result(level) && !playback.showingResult(level), "Starting level shows a result");
    playback.click(level, Cell{4,6});
    playback.click(level, Cell{5,6});
    if (won) {
        playback.update(level, 100);
        check(!playback.result(level), "Incomplete objective shows a result");
        playback.click(level, Cell{5,3});
        playback.click(level, Cell{6,3});
    }
    // Walk only as far as the terminal UI stage. Board has already committed
    // its final counters, yet no result page may replace any ordinary rule phase.
    const auto terminal = won ? Board::Stage::Victory : Board::Stage::Defeat;
    int stages = 0;
    while (playback.current()->stage != terminal) {
        check(level.state() == GameState::Playing && !playback.result(level)
              && !playback.showingResult(level), "Result exposed during gameplay animations");
        const auto duration = BoardPlayback::duration(playback.current()->stage);
        playback.update(level, duration - playback.progress()*duration + 1e-9);
        check(++stages < 100, "Result fixture stuck");
    }
    check(stages > 0, "Result skipped gameplay resolution");
    check(playback.result(level) == (won ? GameState::Won : GameState::Lost), "Wrong result selected");
    check(!playback.showingResult(level), "New page appeared before outgoing fade");
    playback.update(level, 0.1);
    check(!playback.showingResult(level), "Outgoing fade ended early");
    // Cross the outgoing-fade boundary without completing the whole terminal
    // stage: page visibility and final Level publication are distinct gates.
    playback.update(level, 0.11);
    check(playback.showingResult(level) && level.state() == GameState::Playing,
          "Result page failed to enter during UI-only trace stage");
    const auto final = level.board().tiles();
    playback.click(level, Cell{0,0});
    playback.click(level, Cell{0,1});
    check(level.board().tiles() == final && !playback.selected(), "Result accepted gameplay input");
    check(level.board().delivered() == (won ? 1 : 0) && level.board().moves() == (won ? 2u : 1u),
          "Result summary counters incorrect");
    // Partition the same remaining time into different frame sizes to ensure the
    // result gate is elapsed-time based, not dependent on update-call counts.
    auto split = playback;
    auto twin = level;
    playback.update(level, 0.39 + 1e-7);
    for (int i = 0; i < 40; ++i) split.update(twin, 0.01);
    check(playback.acceptsInput() && split.acceptsInput() && level.state() == twin.state()
          && level.state() == (won ? GameState::Won : GameState::Lost), "Transition depends on frame partition");
    check(playback.showingResult(level), "Settled result page disappeared");
    // The application resets both owners. Clearing Level alone would not discard
    // selection, trace cursor, sound queue or result-character clock in playback.
    level.restart();
    playback = BoardPlayback{};
    check(!playback.showingResult(level) && !playback.result(level) && level.acceptsInput()
          && !playback.selected() && !playback.current() && level.board().tiles() == initial
          && level.board().moves() == 0 && level.board().delivered() == 0
          && level.remainingMoves() == definition.moveLimit, "Restart left stale result/animation/counters");
}

/**
 * Checks full result-page rectangles and replay hit areas in both aspect modes,
 * including scaled/minimized windows. Character and button may share vertical
 * space in landscape, but their rectangles must remain spatially disjoint.
 */
void layout() {
    for (auto dimensions : {Cell{480,800}, Cell{800,480}, Cell{960,1600}, Cell{1600,960}, Cell{320,480}}) {
        const auto page = ResultLayout::fit(dimensions.row, dimensions.column);
        for (const auto rect : {page.illustration, page.character, page.restart}) {
            check(rect.x >= 0 && rect.y >= 0 && rect.x + rect.width <= dimensions.row
                  && rect.y + rect.height <= dimensions.column, "Result content outside resized window");
        }
        const auto& button = page.restart;
        check(button.contains(button.x + button.width/2, button.y + button.height/2), "Result button center missed");
        check(!button.contains(button.x-1, button.y) && !button.contains(button.x+button.width, button.y),
              "Result button accepts outside clicks");
        check(std::abs(page.illustration.width - page.illustration.height) < 0.01f,
              "Square illustration distorted");
        check(page.character.y + page.character.height <= page.restart.y
              || page.character.x + page.character.width <= page.restart.x
              || page.restart.x + page.restart.width <= page.character.x,
              "Character overlaps restart control");
    }
    const auto a = ResultLayout::fit(480,800), b = ResultLayout::fit(960,1600);
    check(b.restart.x == a.restart.x*2 && b.restart.y == a.restart.y*2
          && b.illustration.width == a.illustration.width*2, "Result hit test/render scale mismatch");
    check(!ResultLayout::fit(0,0).restart.contains(0,0), "Minimized result accepts input");
}

int main() {
    try {
        resultTiming(true);
        resultTiming(false);
        layout();
        std::cout << "Result timing, summary, input gate, restart, frame partition and responsive layout passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
