import 'kitchen_feed.dart';

/// web 빌드에는 `dart:io` Socket이 없어서 SafetyCore에 붙지 않고 데모 피드를 쓴다.
KitchenFeed? coreFeed(String host, int port) => null;
