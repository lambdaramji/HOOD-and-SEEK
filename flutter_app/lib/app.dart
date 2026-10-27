import 'dart:typed_data';

import 'package:flutter/material.dart';

import 'audio/clip_player.dart';
import 'brand.dart';
import 'data/kitchen_feed.dart';
import 'screens/dashboard_screen.dart';
import 'widgets/hs.dart';

class HoodSeekApp extends StatelessWidget {
  const HoodSeekApp({
    super.key,
    this.feed,
    this.brand = BrandImages.none,
    this.clips = const SilentClipPlayer(),
    this.video,
  });

  final KitchenFeed? feed;
  final BrandImages brand;
  final ClipPlayer clips;
  final Stream<Uint8List>? video;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'HOOD&SEEK',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        brightness: Brightness.light,
        fontFamily: Hs.text,
        fontFamilyFallback: Hs.fallback,
        colorScheme: ColorScheme.fromSeed(
          seedColor: Hs.t1,
          primary: Hs.t1,
          surface: Hs.card,
        ),
        scaffoldBackgroundColor: Hs.bg,
      ),
      home:
          DashboardScreen(feed: feed, brand: brand, clips: clips, video: video),
    );
  }
}
