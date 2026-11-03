/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QVector>
#include <optional>
#include <memory>

struct xkb_keymap;

class KWinKeyboard
{
public:
	struct Key { quint32 code; QVector<quint32> modifiers; };
	KWinKeyboard();
	~KWinKeyboard();
	bool setKeymap(const QByteArray& data);
	bool isValid() const;
	std::optional<Key> translate(quint32 symbol) const;
private:
	struct Deleter { void operator()(xkb_keymap* map) const; };
	std::unique_ptr<xkb_keymap, Deleter> m_keymap;
};
