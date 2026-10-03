package com.github.druk.dnssd;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Handler;

import org.junit.Test;
import org.mockito.ArgumentCaptor;

/**
 * The embedded responder exits a few seconds after the last operation stops, unless a new one
 * starts before then. This failed to cancel because init() removed a different Runnable from the
 * one exit() had posted (a method reference evaluated at two call sites is two objects), so the
 * responder shut down under operations that had started since. DNSSDEmbedded itself loads the
 * native library in its constructor, so the timer is tested on its own.
 */
public class DNSSDEmbeddedExitTimerTest {

    private final Handler handler = mock(Handler.class);
    private final Runnable exit = () -> { };
    private final DNSSDEmbedded.ExitTimer timer = new DNSSDEmbedded.ExitTimer(handler, 5000, exit);

    @Test
    public void cancelRemovesTheRunnableThatWasPosted() {
        timer.schedule();
        ArgumentCaptor<Runnable> posted = ArgumentCaptor.forClass(Runnable.class);
        verify(handler).postDelayed(posted.capture(), eq(5000L));

        timer.cancel();

        // schedule() removes before it posts, cancel() removes again: always the posted Runnable
        verify(handler, times(2)).removeCallbacks(same(posted.getValue()));
    }

    @Test
    public void schedulingAgainLeavesASinglePendingExit() {
        timer.schedule();
        timer.schedule();

        ArgumentCaptor<Runnable> posted = ArgumentCaptor.forClass(Runnable.class);
        verify(handler, times(2)).postDelayed(posted.capture(), anyLong());
        // Each post was preceded by the removal of the same Runnable, so at most one exit is pending
        verify(handler, times(2)).removeCallbacks(same(posted.getValue()));
    }
}
