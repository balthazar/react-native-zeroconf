package com.github.druk.dnssd;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

/**
 * The embedded responder driven from several threads, as DnssdImpl does: operations start on a
 * worker thread, the loop thread delivers the callbacks, and the main thread stops operations.
 * A failure here is often the test process crashing; run with `adb shell setprop debug.checkjni 1`
 * so that JNI misuse aborts with a clear message.
 */
@RunWith(AndroidJUnit4.class)
public class DNSSDEmbeddedTest {

    // mDNS core is global to the process, so the tests share one instance. Short exit delay for exitTimer().
    private static final DNSSDEmbedded dnssd =
            new DNSSDEmbedded(InstrumentationRegistry.getInstrumentation().getTargetContext(), 1500);

    private static final BrowseListener NO_BROWSE = new BrowseListener() {
        @Override public void serviceFound(DNSSDService b, int f, int i, String s, String t, String d) { }
        @Override public void serviceLost(DNSSDService b, int f, int i, String s, String t, String d) { }
        @Override public void operationFailed(DNSSDService s, int err) { }
    };

    private static DNSSDRegistration register(int ifIndex, String name, String type, CountDownLatch registered) throws DNSSDException {
        return dnssd.register(0, ifIndex, name, type, null, null, 9000, null, new RegisterListener() {
            @Override public void serviceRegistered(DNSSDRegistration r, int flags, String n, String t, String d) {
                registered.countDown();
            }
            @Override public void operationFailed(DNSSDService s, int err) { }
        });
    }

    // queryRecord(autoStop) on a name with several answers: the first answer posts stop() to the main
    // thread while the loop thread is still delivering the other records of the same answer. The answer
    // can also come back before queryRecord() has returned.
    @Test
    public void queryStoppedWhileItsAnswerIsDelivered() throws Exception {
        List<DNSSDRegistration> registrations = new ArrayList<>();
        CountDownLatch registered = new CountDownLatch(8);
        for (int i = 0; i < 8; i++) registrations.add(register(0, "zcquery-" + i, "_zcquery._tcp", registered));
        assertTrue(registered.await(15, TimeUnit.SECONDS));
        Thread.sleep(3000); // probing and announcing

        try {
            for (int round = 0; round < 20; round++) {
                AtomicInteger answers = new AtomicInteger();
                dnssd.queryRecord(0, 0, "_zcquery._tcp.local.", 12 /* PTR */, 1 /* IN */, true, new QueryListener() {
                    @Override public void queryAnswered(DNSSDService q, int flags, int ifi, String name, int rrtype, int rrclass, byte[] rdata, int ttl) {
                        answers.incrementAndGet();
                    }
                    @Override public void operationFailed(DNSSDService s, int err) { }
                });
                Thread.sleep(300);
                assertTrue("round " + round + " got no answer", answers.get() > 0);
            }
        } finally {
            for (DNSSDRegistration r : registrations) r.stop();
        }
    }

    // A local-only registration is acknowledged from inside DNSServiceRegister(), on the calling thread.
    @Test
    public void localOnlyRegistrationAcknowledgedOnTheCallingThread() throws Exception {
        CountDownLatch registered = new CountDownLatch(1);
        DNSSDRegistration r = register(-1, "zclocal", "_zclocal._tcp", registered);
        try {
            assertTrue(registered.await(10, TimeUnit.SECONDS));
        } finally {
            r.stop();
        }
    }

    // An operation that starts after the last one stopped, but before the exit timer fires, keeps the
    // responder running.
    @Test
    public void operationStartedBeforeTheExitTimerKeepsTheResponder() throws Exception {
        DNSSDService first = dnssd.browse(0, 0, "_zctimer._tcp", "", NO_BROWSE);
        Thread.sleep(500);
        first.stop();                       // the exit is due in 1.5 s
        Thread.sleep(300);

        CountDownLatch found = new CountDownLatch(1);
        DNSSDService second = dnssd.browse(0, 0, "_zctimer._tcp", "", new BrowseListener() {
            @Override public void serviceFound(DNSSDService b, int f, int i, String s, String t, String d) { found.countDown(); }
            @Override public void serviceLost(DNSSDService b, int f, int i, String s, String t, String d) { }
            @Override public void operationFailed(DNSSDService s, int err) { }
        });
        Thread loop = loopThread();
        Thread.sleep(3000);                 // well past the exit
        assertTrue("the responder exited under a running browse", loop.isAlive());
        assertEquals(loop, loopThread());

        DNSSDRegistration r = register(0, "zctimer-probe", "_zctimer._tcp", new CountDownLatch(1));
        try {
            assertTrue("the browse stopped receiving", found.await(8, TimeUnit.SECONDS));
        } finally {
            r.stop();
            second.stop();
        }
    }

    private static Thread loopThread() throws Exception {
        Field f = DNSSDEmbedded.class.getDeclaredField("mThread");
        f.setAccessible(true);
        synchronized (DNSSDEmbedded.class) {
            return (Thread) f.get(dnssd);
        }
    }
}
