import 'dart:ffi';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:sqlite3/open.dart' as sqlite_open;
import 'package:sqlite3/sqlite3.dart';

/// 기록 탭 DB. Pi 5 앱 안의 SQLite 파일 하나 — 서버가 없다. `--dart-define=HISTORY_DB=<경로>`로 바꾼다.
/// 폴더는 후드 케어와 같은 `/data/hoodseek/app`(root가 `chmod 0777`로 만든 곳, #158·#161).
/// Core가 보낸 state로 앱이 만든 표시 기록만 남긴다. 판정·계약과 무관하고, 열지 못하면 앱 메모리로만 센다.
const historyDbFile = String.fromEnvironment('HISTORY_DB',
    defaultValue: '/data/hoodseek/app/hoodseek.db');

/// 타임라인 한 줄. 색·종류는 이름 문자열로 둔다(화면 enum과 떨어뜨리려고).
class StoredEvent {
  const StoredEvent(this.at, this.tone, this.text, this.trailing, this.kind);

  final DateTime at;
  final String tone;
  final String text;
  final String trailing;
  final String kind;
}

/// 끝난 조리 1회 요약. 띠(0.5 s 표본)는 저장하지 않고 요약 한 줄만.
class StoredSession {
  const StoredSession(this.start, this.end, this.summary);

  final DateTime start;
  final DateTime end;
  final String summary;
}

/// 쓰기·읽기 실패는 앱을 멈추지 않는다. 한 번 실패하면 [failed]가 되고 그 뒤로는 아무것도 하지 않는다.
class HistoryDb {
  HistoryDb._(this._db);

  final Database _db;
  bool failed = false;

  /// 오래된 줄은 열 때 지운다. 하루 수백 줄이라 몇 주 치.
  static const keepEvents = 5000;
  static const keepSessions = 500;

  /// 못 열면(폴더 없음·권한·`libsqlite3` 없음) null.
  ///
  /// 앱 계정은 설치마다 바뀌고(5005 → 5009) umask 0022라, 그대로 두면 다음 설치의 계정이 읽기만 하고
  /// 쓰지 못한다 [실측 10/26 정인 Pi 5: `attempt to write a readonly database (8)`]. 그래서 처음
  /// 만들 때 `chmod 666`으로 열어 두고(저널은 SQLite가 DB 파일 권한을 따라 만든다), 이미 남의 0644
  /// 파일이면 0777 폴더라 이름은 바꿀 수 있으니 `.old`로 옮기고 새로 만든다.
  static HistoryDb? open([String path = historyDbFile]) {
    try {
      _useSystemLibrary();
      if (path == ':memory:') {
        return HistoryDb._(_setUp(sqlite3.openInMemory()));
      }
      final created = !File(path).existsSync();
      try {
        final db = HistoryDb._(_setUp(sqlite3.open(path)));
        if (created) db._shareFile(path);
        return db;
      } on SqliteException catch (error) {
        if (!error.toString().contains('readonly')) rethrow;
        debugPrint('기록 DB가 읽기 전용이라 새로 만듭니다 ($path): $error');
        for (final suffix in ['', '-journal', '-wal', '-shm']) {
          final old = File('$path$suffix');
          if (old.existsSync()) old.renameSync('$path$suffix.old');
        }
        return HistoryDb._(_setUp(sqlite3.open(path))).._shareFile(path);
      }
    } on Object catch (error) {
      debugPrint('기록 DB를 열지 못했습니다 ($path): $error');
      return null;
    }
  }

  /// 표를 만들고 오래된 줄을 지운다. 쓰기라 읽기 전용 파일이면 여기서 [SqliteException].
  /// 순서는 `id`로 센다 — 시계를 맞추지 않고 켜면(1970년) 날짜가 틀려도 최신부터 읽는 것은 맞다.
  static Database _setUp(Database db) {
    try {
      db.execute('''
        CREATE TABLE IF NOT EXISTS events(
          id INTEGER PRIMARY KEY, at_ms INTEGER NOT NULL, tone TEXT NOT NULL,
          text TEXT NOT NULL, trailing TEXT NOT NULL, kind TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS sessions(
          id INTEGER PRIMARY KEY, start_ms INTEGER NOT NULL, end_ms INTEGER NOT NULL,
          summary TEXT NOT NULL);
        DELETE FROM events WHERE id <= (SELECT MAX(id) FROM events) - $keepEvents;
        DELETE FROM sessions WHERE id <= (SELECT MAX(id) FROM sessions) - $keepSessions;
      ''');
      return db;
    } on Object {
      db.dispose();
      rethrow;
    }
  }

  /// 다음 설치의 계정도 쓰게 한다. `dart:io`에 chmod가 없어 BusyBox `chmod`를 부른다.
  void _shareFile(String path) {
    try {
      Process.runSync('chmod', ['666', path]);
    } on Object catch (error) {
      debugPrint('기록 DB 권한을 바꾸지 못했습니다 ($path): $error');
    }
  }

  /// Pi 5·CI 리눅스에는 개발용 `libsqlite3.so` 링크가 없을 수 있어 `.so.0`을 먼저 찾는다.
  static void _useSystemLibrary() {
    sqlite_open.open.overrideFor(sqlite_open.OperatingSystem.linux, () {
      for (final name in ['libsqlite3.so.0', 'libsqlite3.so']) {
        try {
          return DynamicLibrary.open(name);
        } on Object {
          continue;
        }
      }
      throw StateError('libsqlite3를 찾지 못했습니다');
    });
  }

  void addEvent(StoredEvent e) => _write(() => _db.execute(
        'INSERT INTO events(at_ms, tone, text, trailing, kind) VALUES (?, ?, ?, ?, ?)',
        [e.at.millisecondsSinceEpoch, e.tone, e.text, e.trailing, e.kind],
      ));

  void addSession(StoredSession s) => _write(() => _db.execute(
        'INSERT INTO sessions(start_ms, end_ms, summary) VALUES (?, ?, ?)',
        [
          s.start.millisecondsSinceEpoch,
          s.end.millisecondsSinceEpoch,
          s.summary
        ],
      ));

  /// 최신이 앞.
  List<StoredEvent> recentEvents(int limit) => _read(
      () => [
            for (final r in _db.select(
                'SELECT at_ms, tone, text, trailing, kind FROM events ORDER BY id DESC LIMIT ?',
                [limit]))
              StoredEvent(
                DateTime.fromMillisecondsSinceEpoch(r['at_ms'] as int),
                r['tone'] as String,
                r['text'] as String,
                r['trailing'] as String,
                r['kind'] as String,
              ),
          ],
      const []);

  /// 최신이 앞.
  List<StoredSession> recentSessions(int limit) => _read(
      () => [
            for (final r in _db.select(
                'SELECT start_ms, end_ms, summary FROM sessions ORDER BY id DESC LIMIT ?',
                [limit]))
              StoredSession(
                DateTime.fromMillisecondsSinceEpoch(r['start_ms'] as int),
                DateTime.fromMillisecondsSinceEpoch(r['end_ms'] as int),
                r['summary'] as String,
              ),
          ],
      const []);

  void close() {
    try {
      _db.dispose();
    } on Object {
      // 닫는 중 실패는 무시한다.
    }
  }

  void _write(void Function() run) {
    if (failed) return;
    try {
      run();
    } on Object catch (error) {
      failed = true;
      debugPrint('기록 DB에 쓰지 못했습니다: $error');
    }
  }

  T _read<T>(T Function() run, T empty) {
    if (failed) return empty;
    try {
      return run();
    } on Object catch (error) {
      failed = true;
      debugPrint('기록 DB를 읽지 못했습니다: $error');
      return empty;
    }
  }
}
