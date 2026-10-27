import 'hood_care.dart';

/// 웹 빌드에는 파일이 없어 `--dart-define`도 쓰지 않는다. 인터페이스만 `hood_care_store.dart`와 같다.
const hoodCareFile = String.fromEnvironment('HOOD_CARE_FILE',
    defaultValue: '/data/hoodseek/app/hood-care.json');

/// 웹 데모: 저장 없이 앱을 켠 뒤만 센다.
class HoodCareStore {
  HoodCareStore([this.path = hoodCareFile]);

  final String path;

  Future<(HoodCare, String?)> load() async => (HoodCare(), hoodCareSaveFailed);

  Future<bool> save(HoodCare care) async => false;
}
