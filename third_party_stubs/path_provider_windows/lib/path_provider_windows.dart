class PathProviderWindows {
  static void registerWith() {}

  Future<String?> getApplicationSupportPath() async => '.';
}