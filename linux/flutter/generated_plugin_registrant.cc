//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <flutter_blue_plus_linux/flutter_blue_plus_linux.h>
#include <path_provider_linux/path_provider_linux.h>

void fl_register_plugins(FlPluginRegistry* registry) {
  g_autoptr(FlPluginRegistrar) flutter_blue_plus_linux_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "FlutterBluePlusLinux");
  flutter_blue_plus_linux_register_with_registrar(flutter_blue_plus_linux_registrar);
  g_autoptr(FlPluginRegistrar) path_provider_linux_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "PathProviderLinux");
  path_provider_linux_register_with_registrar(path_provider_linux_registrar);
}
