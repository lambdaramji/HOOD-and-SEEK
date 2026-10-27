#!/bin/sh
# Manual device test; never supplies a Core address, --publish or --capture-raw.
set -eu
: "${VOICE_TEST_BINARY:?set candidate binary}"
: "${VOICE_TEST_MODEL_DIR:?set existing SenseVoice directory}"
: "${VOICE_TEST_VAD_MODEL:?set existing Silero model}"
: "${VOICE_TEST_SOURCE:?set requested PulseAudio source name}"
: "${VOICE_TEST_OTHER_SOURCE:?set a different available input for the drift test}"
test "$VOICE_TEST_SOURCE" != "$VOICE_TEST_OTHER_SOURCE"
test -z "$(pactl list short source-outputs)" || {
  echo 'another recording is active; leave it unchanged' >&2; exit 1;
}
test_source_index=$(pactl list short sources | awk -v name="$VOICE_TEST_SOURCE" '$2 == name {print $1}')
test_other_index=$(pactl list short sources | awk -v name="$VOICE_TEST_OTHER_SOURCE" '$2 == name {print $1}')
test -n "$test_source_index" && test -n "$test_other_index"
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/hoodseek-pulse-guard.XXXXXX")
test_pid=
trap 'if test -n "$test_pid"; then kill "$test_pid" 2>/dev/null || true; wait "$test_pid" 2>/dev/null || true; fi' EXIT HUP INT TERM

for test_attempt in 1 2 3; do
  test_log="$test_dir/start-$test_attempt.log"
  "$VOICE_TEST_BINARY" --sensevoice-dir "$VOICE_TEST_MODEL_DIR" --vad-model "$VOICE_TEST_VAD_MODEL" \
    --pulse-source "$VOICE_TEST_SOURCE" --seconds 2 > "$test_log" 2>&1 &
  test_pid=$!
  test_wait=0
  while ! grep -q '^event=capture_ready ' "$test_log"; do
    kill -0 "$test_pid" 2>/dev/null || { cat "$test_log" >&2; exit 1; }
    test_wait=$((test_wait + 1))
    test "$test_wait" -lt 150 || { echo 'capture readiness timeout' >&2; exit 1; }
    sleep 0.1
  done
  test_stream=$(sed -n 's/.* source_output_index=\([0-9][0-9]*\)$/\1/p' "$test_log")
  test -n "$test_stream"
  test_actual=$(pactl list short source-outputs | awk -v stream="$test_stream" '$1 == stream {print $2}')
  test "$test_actual" = "$test_source_index"
  if test "$test_attempt" = 3; then
    # Deliberately move only the source-output announced by this candidate.
    pactl move-source-output "$test_stream" "$VOICE_TEST_OTHER_SOURCE"
    if wait "$test_pid"; then echo 'capture drift did not stop the node' >&2; exit 1; fi
    test_pid=
    grep -q 'capture source changed; processing and publication stopped' "$test_log"
  else
    wait "$test_pid"
    test_pid=
    grep -q 'transmitted=0' "$test_log"
  fi
done

# An unavailable requested source must never fall back to another microphone.
test_log="$test_dir/missing-source.log"
if "$VOICE_TEST_BINARY" --sensevoice-dir "$VOICE_TEST_MODEL_DIR" --vad-model "$VOICE_TEST_VAD_MODEL" \
    --pulse-source "hoodseek-missing-source-$$" --seconds 1 > "$test_log" 2>&1; then
  echo 'missing source unexpectedly succeeded' >&2; exit 1
fi
if grep -Eq '^event=(capture_ready|audio_level|asr) ' "$test_log"; then
  echo 'unverified input reached the audio pipeline' >&2; exit 1
fi
printf 'pulse_source_guard: startup/restart, drift stop, missing source passed; logs=%s\n' "$test_dir"
