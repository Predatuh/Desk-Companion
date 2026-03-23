import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'providers/desk_companion_controller.dart';
import 'screens/desk_companion_studio_screen.dart';
import 'theme/companion_theme.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const DeskCompanionApp());
}

class DeskCompanionApp extends StatelessWidget {
  const DeskCompanionApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => DeskCompanionController(),
      child: MaterialApp(
        title: 'Desk Companion',
        debugShowCheckedModeBanner: false,
        theme: CompanionTheme.themeData,
        home: const DeskCompanionStudioScreen(),
      ),
    );
  }
}
