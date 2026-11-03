/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "KWinKeyboard.h"
#include <QtTest>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>
#include <cstdlib>

static QByteArray keymap()
{
	auto* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	xkb_rule_names names{};
	names.rules = "evdev";
	names.model = "pc105";
	names.layout = "us,ru";
	auto* map = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	QByteArray data;
	if (map)
	{
		auto* text = xkb_keymap_get_as_string(map, XKB_KEYMAP_FORMAT_TEXT_V1);
		data = QByteArray(text);
		data.append('\0');
		std::free(text);
		xkb_keymap_unref(map);
	}
	xkb_context_unref(context);
	return data;
}

class KWinKeyboardTest : public QObject
{
	Q_OBJECT
private Q_SLOTS:
	void translatesPhysicalKeysAndShiftedSymbols()
	{
		KWinKeyboard keyboard;
		QVERIFY(keyboard.setKeymap(keymap()));
		const auto a = keyboard.translate(XKB_KEY_a);
		QVERIFY(a.has_value());
		QCOMPARE(a->code, uint(KEY_A));
		QVERIFY(a->modifiers.isEmpty());
		const auto upper = keyboard.translate(XKB_KEY_A);
		QVERIFY(upper.has_value());
		QCOMPARE(upper->code, uint(KEY_A));
		QCOMPARE(upper->modifiers, QVector<quint32>{KEY_LEFTSHIFT});
		const auto enter = keyboard.translate(XKB_KEY_Return);
		QVERIFY(enter.has_value());
		QCOMPARE(enter->code, uint(KEY_ENTER));
	}
	void acceptsRfbUnicodeForLegacyCyrillicSymbols()
	{
		KWinKeyboard keyboard;
		QVERIFY(keyboard.setKeymap(keymap()));
		const auto letter = keyboard.translate(0x01000444); // Cyrillic small ef, physical A.
		QVERIFY(letter.has_value());
		QCOMPARE(letter->code, uint(KEY_A));
	}
	void translatesKeypadDigitsWithoutTogglingNumLock_data()
	{
		QTest::addColumn<quint32>("symbol");
		QTest::addColumn<quint32>("code");
		QTest::newRow("zero") << quint32(XKB_KEY_KP_0) << quint32(KEY_KP0);
		QTest::newRow("one") << quint32(XKB_KEY_KP_1) << quint32(KEY_KP1);
		QTest::newRow("nine") << quint32(XKB_KEY_KP_9) << quint32(KEY_KP9);
		QTest::newRow("decimal") << quint32(XKB_KEY_KP_Decimal) << quint32(KEY_KPDOT);
		QTest::newRow("end") << quint32(XKB_KEY_KP_End) << quint32(KEY_KP1);
	}
	void translatesKeypadDigitsWithoutTogglingNumLock()
	{
		QFETCH(quint32, symbol);
		QFETCH(quint32, code);
		KWinKeyboard keyboard;
		QVERIFY(keyboard.setKeymap(keymap()));
		const auto key = keyboard.translate(symbol);
		QVERIFY(key.has_value());
		QCOMPARE(key->code, code);
		QVERIFY(key->modifiers.isEmpty());
	}
	void rejectsMalformedAndOversizedKeymaps()
	{
		KWinKeyboard keyboard;
		QVERIFY(!keyboard.setKeymap(QByteArrayLiteral("invalid")));
		QVERIFY(!keyboard.setKeymap(QByteArray(1024 * 1024 + 1, 'x')));
		QVERIFY(!keyboard.translate(XKB_KEY_a).has_value());
		QVERIFY(keyboard.setKeymap(keymap()));
		QVERIFY(!keyboard.translate(0xffffffff).has_value());
		QVERIFY(!keyboard.setKeymap(QByteArrayLiteral("invalid\0")));
		QVERIFY(!keyboard.translate(XKB_KEY_a).has_value());
	}
};

QTEST_GUILESS_MAIN(KWinKeyboardTest)
#include "KWinKeyboardTest.moc"
