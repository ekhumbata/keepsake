#ifndef TEST_CONSOLE_RUNTIME_H_
#define TEST_CONSOLE_RUNTIME_H_

// The board test console (#15): lets tools/board/ on a computer check a plugged-in board.
// Reads "kst <command>" lines from the USB serial port and answers with framed JSON
// (board_test's wire format). See docs/board-tests.md for the commands. Built only with
// CONFIG_KEEPSAKE_TEST_CONSOLE.
namespace test_console_runtime {

// Starts the console task. Call once startup is complete.
void Start();

}  // namespace test_console_runtime

#endif  // TEST_CONSOLE_RUNTIME_H_
