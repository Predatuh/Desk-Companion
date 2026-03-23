// This is a basic Flutter widget test.
//
// To perform an interaction with a widget in your test, use the WidgetTester
// utility in the flutter_test package. For example, you can send tap and scroll
// gestures. You can also use WidgetTester to find child widgets in the widget
// tree, read text, and verify that the values of widget properties are correct.

import 'package:flutter_test/flutter_test.dart';

import 'package:desk_companion/main.dart';

void main() {
  testWidgets('renders desk companion shell', (WidgetTester tester) async {
    await tester.pumpWidget(const DeskCompanionApp());
    await tester.pumpAndSettle();

    expect(find.text('Desk Companion'), findsWidgets);
    expect(find.text('Sticky note'), findsOneWidget);
  });
}
