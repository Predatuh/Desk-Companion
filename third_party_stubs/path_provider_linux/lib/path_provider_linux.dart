class PathProviderLinux {
  static void registerWith() {}

  Future<String?> getApplicationSupportPath() async => '.';
}