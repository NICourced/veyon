/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ProcessIdentity.h"

#include <QtTest>
#include <unistd.h>

namespace
{
ProcessIdentity identity(uint pid, uint parent, uint uid, quint64 inode, const QString& path)
{
	ProcessIdentity result;
	result.pid = pid;
	result.parentPid = parent;
	result.uid = uid;
	result.startTime = 12345;
	result.device = 1;
	result.inode = inode;
	result.executable = path;
	result.trustedExecutable = true;
	return result;
}
}

class LoginProcessIdentityTest : public QObject
{
	Q_OBJECT
private Q_SLOTS:
	void parseKernelStat();
	void rejectMalformedStat();
	void readCurrentProcess();
	void rejectReusedPidOrExecutable();
	void acceptOnlyDirectLightDmWrapper();
};

void LoginProcessIdentityTest::parseKernelStat()
{
	QByteArray data = QByteArrayLiteral("42 (name with ) parentheses) S 7");
	for (int field = 5; field < 22; ++field)
	{
		data += QByteArrayLiteral(" 0");
	}
	data += QByteArrayLiteral(" 18446744073709551614 0\n");
	uint pid = 0, parent = 0;
	quint64 start = 0;
	QVERIFY(ProcessIdentity::parseStat(data, pid, parent, start));
	QCOMPARE(pid, 42U);
	QCOMPARE(parent, 7U);
	QCOMPARE(start, Q_UINT64_C(18446744073709551614));
}

void LoginProcessIdentityTest::rejectMalformedStat()
{
	uint pid = 0, parent = 0;
	quint64 start = 0;
	QVERIFY(!ProcessIdentity::parseStat({}, pid, parent, start));
	QVERIFY(!ProcessIdentity::parseStat(QByteArrayLiteral("42 (missing fields) S 7"), pid, parent, start));
	QVERIFY(!ProcessIdentity::parseStat(QByteArrayLiteral("42 missing parentheses"), pid, parent, start));
}

void LoginProcessIdentityTest::readCurrentProcess()
{
	const auto first = ProcessIdentity::read(uint(::getpid()));
	const auto second = ProcessIdentity::read(uint(::getpid()));
	QVERIFY(first.isValid());
	QCOMPARE(first.pid, uint(::getpid()));
	QCOMPARE(first.parentPid, uint(::getppid()));
	QCOMPARE(first.uid, uint(::getuid()));
	QVERIFY(first.sameProcess(second));
	QVERIFY(!ProcessIdentity::read(0).isValid());
}

void LoginProcessIdentityTest::rejectReusedPidOrExecutable()
{
	const auto original = identity(42, 7, 100, 9, QStringLiteral("/usr/bin/veyon-login-helper"));
	auto changed = original;
	QVERIFY(original.sameProcess(changed));
	++changed.startTime;
	QVERIFY(!original.sameProcess(changed));
	changed = original;
	++changed.inode;
	QVERIFY(!original.sameProcess(changed));
	changed = original;
	++changed.uid;
	QVERIFY(!original.sameProcess(changed));
	changed = original;
	++changed.parentPid;
	QVERIFY(!original.sameProcess(changed));
	QVERIFY(!original.sameProcess({}));
}

void LoginProcessIdentityTest::acceptOnlyDirectLightDmWrapper()
{
	const auto helper = identity(90, 1, 0, 9, QStringLiteral("/usr/bin/veyon-login-helper"));
	auto wrapper = identity(42, 7, 100, 9, helper.executable);
	auto parent = identity(7, 2, 0, 10, QStringLiteral("/usr/sbin/lightdm"));
	QVERIFY(wrapper.isLightDmWrapper(parent, helper, 7, 100));
	// Same UID/session and even the correct executable do not authorize a
	// second wrapper forked by the UI or another process in the greeter session.
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 8, 100));
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 101));
	parent.uid = 100;
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 100));
	parent.uid = 0;
	parent.executable = QStringLiteral("/usr/bin/other-root-process");
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 100));
	parent.executable = QStringLiteral("/usr/sbin/lightdm");
	wrapper.trustedExecutable = false;
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 100));
	wrapper.trustedExecutable = true;
	++wrapper.inode;
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 100));
	--wrapper.inode;
	parent.trustedExecutable = false;
	QVERIFY(!wrapper.isLightDmWrapper(parent, helper, 7, 100));
}

QTEST_GUILESS_MAIN(LoginProcessIdentityTest)
#include "LoginProcessIdentityTest.moc"
