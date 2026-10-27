/// 웹 데모에는 DB가 없다. 인터페이스만 `history_db.dart`와 같다.
const historyDbFile = String.fromEnvironment('HISTORY_DB',
    defaultValue: '/data/hoodseek/app/hoodseek.db');

class StoredEvent {
  const StoredEvent(this.at, this.tone, this.text, this.trailing, this.kind);

  final DateTime at;
  final String tone;
  final String text;
  final String trailing;
  final String kind;
}

class StoredSession {
  const StoredSession(this.start, this.end, this.summary);

  final DateTime start;
  final DateTime end;
  final String summary;
}

class HistoryDb {
  bool failed = false;

  static HistoryDb? open([String path = historyDbFile]) => null;

  void addEvent(StoredEvent e) {}
  void addSession(StoredSession s) {}
  List<StoredEvent> recentEvents(int limit) => const [];
  List<StoredSession> recentSessions(int limit) => const [];
  void close() {}
}
