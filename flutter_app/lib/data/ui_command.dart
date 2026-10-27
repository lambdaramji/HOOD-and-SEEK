/// Flutter → SafetyCore `ui_cmd` (TCP 계약 ui_cmd v2, `contracts/ui_cmd.schema.json`).
///
/// 앱은 요청만 보낸다. 집행 여부(`fan_min` 하한, `allowed_on`, 잠금 해제)는
/// SafetyCore가 판정하고 결과는 다음 `state`로 돌아온다.
library;

enum UiCmd {
  fanSet('FAN_SET'),
  indSet('IND_SET'),
  ackAlert('ACK_ALERT'),
  userLock('USER_LOCK');

  const UiCmd(this.wire);
  final String wire;
}

class UiCommand {
  const UiCommand._(this.cmd, this.value);

  factory UiCommand.fanSet(int level) =>
      UiCommand._(UiCmd.fanSet, _step(level));

  factory UiCommand.indSet(int level) =>
      UiCommand._(UiCmd.indSet, _step(level));

  /// "위험 확인". 위험 잠금 해제의 필요조건 하나일 뿐 잠금을 직접 풀지 못한다.
  const UiCommand.ackAlert() : this._(UiCmd.ackAlert, 0);

  /// 사용자 잠금 켜기·끄기 (D-058). 사용자 잠금만 바꾸고 위험·반려동물 잠금은 풀지 못한다.
  const UiCommand.userLock(bool locked)
      : this._(UiCmd.userLock, locked ? 1 : 0);

  final UiCmd cmd;
  final int value;

  Map<String, Object?> toPayload() => {'cmd': cmd.wire, 'value': value};

  /// envelope `v`. USER_LOCK만 v 2이고 옛 세 명령은 v 1 그대로 — 새 앱을 먼저 올려도
  /// 옛 Core(v 1만 받음)가 터치 명령을 계속 받는다 (contracts/README 「변경 규칙」).
  int get version => cmd == UiCmd.userLock ? 2 : 1;

  @override
  String toString() => '${cmd.wire}($value)';

  static int _step(int level) {
    if (level < 0 || level > 3) {
      throw RangeError.range(level, 0, 3, 'level');
    }
    return level;
  }
}
