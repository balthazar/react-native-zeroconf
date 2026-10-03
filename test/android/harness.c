// Harness for the embedded mDNSResponder of the Android module, run on Linux under AddressSanitizer
// by test/android/run.sh. It drives dnssd_clientshim.c the way the Java side does: the responder's
// event loop runs on its own thread (DNSSDEmbedded's "DNS-SDEmbedded"), and the DNSService* calls
// come from other threads. Everything is registered and queried on the local-only interface, so it
// needs no network. The flow is the one of DnssdImpl.java: register, browse, resolve, query the
// addresses, update the TXT record, unregister, and stop operations from another thread.
//
// Local-only questions are answered from the registered records (AnswerNewLocalOnlyQuestion in
// mDNS.c) rather than from the cache (AnswerNewQuestion, the path of the device crash); the record
// loop and its m->CurrentQuestion guard are the same shape in both.
#include <dns_sd.h>

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// PosixDaemon.c, as called by DNSSDEmbedded through JNISupport.c
int init(void);
int loop(void);
void stopLoop(void);

#define SERVICES 4
#define TYPE "_rnztest._tcp"
#define SUBTYPE "_sub1"
#define DOMAIN "local."

static int failures = 0;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Waits up to `seconds` for the semaphore, returns whether it was posted
static int waitFor(sem_t *sem, double seconds)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)seconds;
    deadline.tv_nsec += (long)((seconds - (time_t)seconds) * 1e9);
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
    while (sem_timedwait(sem, &deadline) != 0) {
        if (errno != EINTR) return 0;
    }
    return 1;
}

// ---------------------------------------------------------------- the loop thread

static pthread_t loopThread, mainThread;
static sem_t loopStarted;
static int initResult = -1;
static double loopStartedAt;

static void *runLoop(void *arg)
{
    (void)arg;
    initResult = init();
    sem_post(&loopStarted);
    if (initResult == 0) loop();
    return NULL;
}

static void startLoop(void)
{
    sem_init(&loopStarted, 0, 0);
    pthread_create(&loopThread, NULL, runLoop, NULL);
    waitFor(&loopStarted, 10);
    loopStartedAt = now();
    check(initResult == 0, "the responder initialised");
}

// Stops the loop and waits for the thread, returns how long that took
static double stopLoopAndJoin(void)
{
    double t = now();
    stopLoop();
    pthread_join(loopThread, NULL);
    return now() - t;
}

// Every callback records its thread. mDNS core delivers from the loop thread, except that a
// local-only registration is acknowledged inside DNSServiceRegister(), on the calling thread.
// Anything else would be a bug (JNISupport.c needs the current thread's JNIEnv).
static int callbacksOnLoopThread = 0, callbacksOnMainThread = 0, callbacksElsewhere = 0;
static void noteThread(void)
{
    pthread_t self = pthread_self();
    if (pthread_equal(self, loopThread)) __atomic_add_fetch(&callbacksOnLoopThread, 1, __ATOMIC_SEQ_CST);
    else if (pthread_equal(self, mainThread)) __atomic_add_fetch(&callbacksOnMainThread, 1, __ATOMIC_SEQ_CST);
    else __atomic_add_fetch(&callbacksElsewhere, 1, __ATOMIC_SEQ_CST);
}

// ---------------------------------------------------------------- registration

static sem_t registered;
static void registerReply(DNSServiceRef ref, DNSServiceFlags flags, DNSServiceErrorType error,
                          const char *name, const char *regtype, const char *domain, void *context)
{
    (void)ref; (void)flags; (void)name; (void)regtype; (void)domain; (void)context;
    noteThread();
    if (error == kDNSServiceErr_NoError) sem_post(&registered);
}

static DNSServiceErrorType registerService(DNSServiceRef *ref, const char *name, const char *regtype, uint16_t port, TXTRecordRef *txt)
{
    return DNSServiceRegister(ref, 0, kDNSServiceInterfaceIndexLocalOnly, name, regtype, DOMAIN, NULL, htons(port),
                              txt ? TXTRecordGetLength(txt) : 0, txt ? TXTRecordGetBytesPtr(txt) : NULL, registerReply, NULL);
}

// ---------------------------------------------------------------- browse

typedef struct {
    sem_t event;
    int adds, removes;
    char names[8][64];       // of the adds, in order
    char lastRemoved[64];
    char regtype[64], domain[64];
    int interfaceIndex;
} Browse;

static void browseReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                        const char *name, const char *regtype, const char *domain, void *context)
{
    Browse *browse = (Browse *)context;
    (void)ref;
    noteThread();
    if (error != kDNSServiceErr_NoError) { printf("     browse error %d\n", error); return; }
    if (flags & kDNSServiceFlagsAdd) {
        if (browse->adds < 8) snprintf(browse->names[browse->adds], 64, "%s", name);
        browse->adds++;
        snprintf(browse->regtype, sizeof(browse->regtype), "%s", regtype);
        snprintf(browse->domain, sizeof(browse->domain), "%s", domain);
        browse->interfaceIndex = (int)interfaceIndex;
    } else {
        snprintf(browse->lastRemoved, sizeof(browse->lastRemoved), "%s", name);
        browse->removes++;
    }
    sem_post(&browse->event);
}

static DNSServiceErrorType startBrowse(DNSServiceRef *ref, Browse *browse, const char *regtype)
{
    memset(browse, 0, sizeof(*browse));
    sem_init(&browse->event, 0, 0);
    return DNSServiceBrowse(ref, 0, kDNSServiceInterfaceIndexLocalOnly, regtype, DOMAIN, browseReply, browse);
}

static int waitForEvents(Browse *browse, int count, double seconds)
{
    for (int i = 0; i < count; i++) if (!waitFor(&browse->event, seconds)) return 0;
    return 1;
}

static int browseHas(const Browse *browse, const char *name)
{
    for (int i = 0; i < browse->adds && i < 8; i++) if (strcmp(browse->names[i], name) == 0) return 1;
    return 0;
}

// ---------------------------------------------------------------- resolve

typedef struct {
    sem_t done;
    char fullname[256], host[256];
    uint16_t port;
    unsigned char txt[256];
    uint16_t txtLen;
} Resolve;

static void resolveReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                         const char *fullname, const char *host, uint16_t port, uint16_t txtLen, const unsigned char *txt, void *context)
{
    Resolve *resolve = (Resolve *)context;
    (void)ref; (void)flags; (void)interfaceIndex;
    noteThread();
    if (error != kDNSServiceErr_NoError) { printf("     resolve error %d\n", error); return; }
    snprintf(resolve->fullname, sizeof(resolve->fullname), "%s", fullname);
    snprintf(resolve->host, sizeof(resolve->host), "%s", host);
    resolve->port = ntohs(port);
    resolve->txtLen = txtLen < sizeof(resolve->txt) ? txtLen : sizeof(resolve->txt);
    memcpy(resolve->txt, txt, resolve->txtLen);
    sem_post(&resolve->done);
}

// Resolves a service like DNSSD.resolve does: the first answer is taken and the operation stopped
static int resolveService(const char *name, const char *regtype, Resolve *resolve)
{
    memset(resolve, 0, sizeof(*resolve));
    sem_init(&resolve->done, 0, 0);
    DNSServiceRef ref = NULL;
    if (DNSServiceResolve(&ref, 0, kDNSServiceInterfaceIndexLocalOnly, name, regtype, DOMAIN, resolveReply, resolve) != kDNSServiceErr_NoError) return 0;
    int ok = waitFor(&resolve->done, 5);
    DNSServiceRefDeallocate(ref);
    return ok;
}

// ---------------------------------------------------------------- records

typedef struct {
    sem_t event;
    int answers;
    uint16_t lastType, lastLength;
    volatile int stopped;    // set after the operation was deallocated: no callback may follow
    int late;
} Query;

static void queryReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                       const char *fullname, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *context)
{
    Query *query = (Query *)context;
    (void)ref; (void)flags; (void)interfaceIndex; (void)fullname; (void)rrclass; (void)rdata; (void)ttl;
    noteThread();
    if (query->stopped) query->late++;
    if (error != kDNSServiceErr_NoError) { printf("     query error %d\n", error); return; }
    query->answers++;
    query->lastType = rrtype;
    query->lastLength = rdlen;
    sem_post(&query->event);
}

static DNSServiceErrorType startQuery(DNSServiceRef *ref, Query *query, const char *name, uint16_t rrtype)
{
    memset(query, 0, sizeof(*query));
    sem_init(&query->event, 0, 0);
    return DNSServiceQueryRecord(ref, 0, kDNSServiceInterfaceIndexLocalOnly, name, rrtype, kDNSServiceClass_IN, queryReply, query);
}

// ---------------------------------------------------------------- a stop from another thread during delivery
//
// A multi-record answer (the PTR records of a browse, or of a PTR query) is delivered in one pass
// of the loop thread, one callback per record. The Java side stops the operation from the main
// thread as soon as the first record arrives (DNSSD.queryRecord with autoStop posts stop() to the
// main thread), while the loop thread is still delivering the rest. Here `stopper` plays the main
// thread: the first callback releases it and waits until it is about to deallocate, then returns.
//
// With the core lock, the stop waits until the loop thread is done with the answer, so every record
// is delivered and the operation is freed afterwards. Without it, the stop lands between two
// records: the remaining ones are dropped, or, when it lands a few instructions later, delivered to
// the freed operation (the DNS-SDEmbedded segfault; AddressSanitizer reports it when the timing
// hits). The checks are on the interleaving, which fails either way.

typedef struct {
    DNSServiceRef ref;
    int answers;
    int answersWhenStopped;
    double deallocateSeconds;    // how long the stop blocked: the rest of the delivery pass
    sem_t stopNow, stopperReady, stopped;
} Race;

static void raceAnswer(Race *race)
{
    if (__atomic_add_fetch(&race->answers, 1, __ATOMIC_SEQ_CST) == 1) {
        sem_post(&race->stopNow);
        waitFor(&race->stopperReady, 5);  // the stopper is now calling DNSServiceRefDeallocate
        usleep(50 * 1000);                // ... and has had time to free the operation, were that possible
    }
}

static void *stopper(void *arg)
{
    Race *race = (Race *)arg;
    if (!waitFor(&race->stopNow, 10)) return NULL;
    sem_post(&race->stopperReady);
    double t = now();
    DNSServiceRefDeallocate(race->ref);
    race->deallocateSeconds = now() - t;
    race->answersWhenStopped = __atomic_load_n(&race->answers, __ATOMIC_SEQ_CST);
    sem_post(&race->stopped);
    return NULL;
}

static void racePtrReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                         const char *fullname, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *context)
{
    (void)ref; (void)flags; (void)interfaceIndex; (void)fullname; (void)rrtype; (void)rrclass; (void)rdlen; (void)rdata; (void)ttl;
    noteThread();
    if (error == kDNSServiceErr_NoError) raceAnswer((Race *)context);
}

static void raceBrowseReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                            const char *name, const char *regtype, const char *domain, void *context)
{
    (void)ref; (void)flags; (void)interfaceIndex; (void)name; (void)regtype; (void)domain;
    noteThread();
    if (error == kDNSServiceErr_NoError) raceAnswer((Race *)context);
}

// Starts `start` with the race as context, stops it from another thread during the first callback,
// and checks that the whole answer of `expected` records was delivered before the stop took effect.
static void stopDuringDelivery(const char *what, int expected, DNSServiceErrorType (*start)(Race *))
{
    Race race;
    memset(&race, 0, sizeof(race));
    sem_init(&race.stopNow, 0, 0);
    sem_init(&race.stopperReady, 0, 0);
    sem_init(&race.stopped, 0, 0);
    pthread_t thread;
    pthread_create(&thread, NULL, stopper, &race);

    char line[160];
    snprintf(line, sizeof(line), "%s: the operation started", what);
    check(start(&race) == kDNSServiceErr_NoError, line);
    snprintf(line, sizeof(line), "%s: it was stopped from another thread during the first callback", what);
    check(waitFor(&race.stopped, 10), line);
    pthread_join(thread, NULL);
    snprintf(line, sizeof(line), "%s: every record of the answer was delivered (%d of %d)", what, race.answers, expected);
    check(race.answers == expected, line);
    snprintf(line, sizeof(line), "%s: the stop waited until the loop thread had delivered the whole answer", what);
    check(race.answersWhenStopped == expected, line);
    printf("     (the stop blocked for %.0f ms)\n", race.deallocateSeconds * 1000);
}

static DNSServiceErrorType startRacePtrQuery(Race *race)
{
    return DNSServiceQueryRecord(&race->ref, 0, kDNSServiceInterfaceIndexLocalOnly, TYPE "." DOMAIN, kDNSServiceType_PTR, kDNSServiceClass_IN, racePtrReply, race);
}

static DNSServiceErrorType startRaceBrowse(Race *race)
{
    return DNSServiceBrowse(&race->ref, 0, kDNSServiceInterfaceIndexLocalOnly, TYPE, DOMAIN, raceBrowseReply, race);
}

// ---------------------------------------------------------------- TXT records

static void buildTxt(TXTRecordRef *txt, const char *version)
{
    TXTRecordCreate(txt, 0, NULL);
    TXTRecordSetValue(txt, "b", 1, "2");
    TXTRecordSetValue(txt, "a", 1, "1");
    TXTRecordSetValue(txt, "u", 5, "caf\xc3\xa9");  // UTF-8 "café"
    TXTRecordSetValue(txt, "v", (uint8_t)strlen(version), version);
}

static int txtValueIs(const unsigned char *txt, uint16_t len, const char *key, const char *expected)
{
    uint8_t valueLen = 0;
    const void *value = TXTRecordGetValuePtr(len, txt, key, &valueLen);
    return value != NULL && valueLen == strlen(expected) && memcmp(value, expected, valueLen) == 0;
}

static int endsWith(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

// ---------------------------------------------------------------- main

int main(void)
{
    mainThread = pthread_self();
    sem_init(&registered, 0, 0);
    char line[200];

    startLoop();

    // ---- publish: four services with a TXT record, from this thread, like DNSSD.register
    TXTRecordRef txt;
    buildTxt(&txt, "1");
    DNSServiceRef registrations[SERVICES];
    double started = now();
    for (int i = 0; i < SERVICES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Harness %d", i + 1);
        if (registerService(&registrations[i], name, TYPE, 8080, &txt) != kDNSServiceErr_NoError) {
            printf("FAIL DNSServiceRegister %d\n", i);
            return 1;
        }
    }
    int allRegistered = 1;
    for (int i = 0; i < SERVICES; i++) if (!waitFor(&registered, 5)) allRegistered = 0;
    check(allRegistered, "services registered from another thread are confirmed within 5 s");
    printf("     (%.0f ms)\n", (now() - started) * 1000);

    // ---- scan: a browse finds them once each (DnssdImpl.scan)
    DNSServiceRef browseRef = NULL;
    Browse browse;
    check(startBrowse(&browseRef, &browse, TYPE) == kDNSServiceErr_NoError, "a browse started");
    check(waitForEvents(&browse, SERVICES, 5), "the browse found the services within 5 s");
    usleep(200 * 1000);
    snprintf(line, sizeof(line), "each service was found exactly once (%d adds)", browse.adds);
    check(browse.adds == SERVICES && browseHas(&browse, "Harness 1") && browseHas(&browse, "Harness 4"), line);
    check(strcmp(browse.regtype, TYPE ".") == 0 && strcmp(browse.domain, DOMAIN) == 0, "with the type and domain the Java side splits on");

    // ---- resolve the first one (DnssdImpl.startResolve -> DNSSD.resolve)
    Resolve resolve;
    check(resolveService("Harness 1", TYPE, &resolve), "the service resolved within 5 s");
    check(strncmp(resolve.fullname, "Harness", 7) == 0 && endsWith(resolve.fullname, "." TYPE "." DOMAIN), "with its full name");
    check(endsWith(resolve.host, "." DOMAIN) && resolve.port == 8080, "its host and port");
    check(resolve.txtLen == TXTRecordGetLength(&txt) && memcmp(resolve.txt, TXTRecordGetBytesPtr(&txt), resolve.txtLen) == 0, "and the TXT record as registered");
    check(txtValueIs(resolve.txt, resolve.txtLen, "u", "caf\xc3\xa9"), "with its UTF-8 value intact");

    // ---- its addresses (DnssdImpl.queryAddresses -> DNSSD.queryRecord A/AAAA with autoStop)
    DNSServiceRef addressRef = NULL;
    Query address;
    check(startQuery(&addressRef, &address, resolve.host, kDNSServiceType_A) == kDNSServiceErr_NoError, "an A query for the host started");
    check(waitFor(&address.event, 5) && address.lastType == kDNSServiceType_A && address.lastLength == 4, "and was answered with an IPv4 address within 5 s");
    DNSServiceRefDeallocate(addressRef);

    // ---- stops from another thread while a multi-record answer is being delivered
    stopDuringDelivery("PTR query", SERVICES, startRacePtrQuery);
    stopDuringDelivery("browse", SERVICES, startRaceBrowse);

    // ---- unregister one while browsing: the browse reports the removal (DnssdImpl: EVENT_REMOVE)
    DNSServiceRefDeallocate(registrations[1]);
    registrations[1] = NULL;
    check(waitFor(&browse.event, 5) && browse.removes == 1 && strcmp(browse.lastRemoved, "Harness 2") == 0, "unregistering a service is reported to the browse within 5 s");
    usleep(200 * 1000);
    check(browse.removes == 1 && browse.adds == SERVICES, "and only that service was removed");

    // ---- update the TXT record (DnssdImpl.updateService -> DNSRecord.update): a fresh resolve sees it
    TXTRecordRef txt2;
    buildTxt(&txt2, "2");
    check(DNSServiceUpdateRecord(registrations[0], NULL, 0, TXTRecordGetLength(&txt2), TXTRecordGetBytesPtr(&txt2), 0) == kDNSServiceErr_NoError, "the TXT record of a published service can be updated");
    check(resolveService("Harness 1", TYPE, &resolve) && txtValueIs(resolve.txt, resolve.txtLen, "v", "2"), "and a fresh resolve returns the new record");
    DNSRecordRef extra = NULL;
    check(DNSServiceAddRecord(registrations[0], &extra, 0, kDNSServiceType_TXT, 1, "", 0) == kDNSServiceErr_Unsupported, "adding records is reported as unsupported, not ignored");
    TXTRecordDeallocate(&txt2);

    // ---- subtypes (react-native-zeroconf's addition to the shim): a subtype browse finds only the service registered with it
    DNSServiceRef subRegistration = NULL;
    check(registerService(&subRegistration, "Harness Sub", TYPE "," SUBTYPE, 8081, NULL) == kDNSServiceErr_NoError && waitFor(&registered, 5), "a service registered with a subtype");
    DNSServiceRef subBrowseRef = NULL;
    Browse subBrowse;
    check(startBrowse(&subBrowseRef, &subBrowse, TYPE "," SUBTYPE) == kDNSServiceErr_NoError && waitForEvents(&subBrowse, 1, 5), "a browse of the subtype found it");
    usleep(200 * 1000);
    snprintf(line, sizeof(line), "and nothing else (%d adds)", subBrowse.adds);
    check(subBrowse.adds == 1 && browseHas(&subBrowse, "Harness Sub"), line);
    DNSServiceRefDeallocate(subBrowseRef);
    check(waitFor(&browse.event, 5) && browseHas(&browse, "Harness Sub"), "while the plain browse of the type found it too");

    // ---- bad parameters come back as errors, without memory errors
    DNSServiceRef bad = NULL;
    check(DNSServiceRegister(&bad, 0, kDNSServiceInterfaceIndexLocalOnly, "Bad", "_zcdns._nope", DOMAIN, NULL, htons(1), 0, NULL, registerReply, NULL) != kDNSServiceErr_NoError, "registering with a bad protocol fails");
    check(DNSServiceBrowse(&bad, 0, kDNSServiceInterfaceIndexLocalOnly, "_a._tcp,_x,_y", DOMAIN, browseReply, &browse) != kDNSServiceErr_NoError, "browsing with two subtypes fails");
    char longName[200];
    memset(longName, 'x', 70); strcpy(longName + 70, "." DOMAIN);
    Query badQuery;
    check(startQuery(&bad, &badQuery, longName, kDNSServiceType_A) != kDNSServiceErr_NoError, "querying a name with an over-long label fails");

    // ---- start and stop queries in quick succession from this thread, as rapid rescans do
    int late = 0, startFailures = 0;
    for (int i = 0; i < 50; i++) {
        DNSServiceRef q = NULL;
        Query query;
        if (startQuery(&q, &query, TYPE "." DOMAIN, kDNSServiceType_PTR) != kDNSServiceErr_NoError) { startFailures++; continue; }
        usleep((i % 5) * 300);
        DNSServiceRefDeallocate(q);
        query.stopped = 1;
        usleep(1000);
        late += query.late;
    }
    check(startFailures == 0 && late == 0, "50 queries started and stopped straight away, none answered after its stop");

    // ---- shut down and start again, as DNSSDEmbedded does after its idle delay
    DNSServiceRefDeallocate(browseRef);
    for (int i = 0; i < SERVICES; i++) if (registrations[i]) DNSServiceRefDeallocate(registrations[i]);
    DNSServiceRefDeallocate(subRegistration);
    double stopSeconds = stopLoopAndJoin();
    snprintf(line, sizeof(line), "the loop stopped within 2 s of being asked to (%.0f ms)", stopSeconds * 1000);
    check(stopSeconds < 2, line);

    startLoop();
    DNSServiceRef again = NULL;
    check(registerService(&again, "Harness Again", TYPE, 8082, &txt) == kDNSServiceErr_NoError && waitFor(&registered, 5), "after a restart, a service registers");
    Browse browseAgain;
    DNSServiceRef browseAgainRef = NULL;
    check(startBrowse(&browseAgainRef, &browseAgain, TYPE) == kDNSServiceErr_NoError && waitForEvents(&browseAgain, 1, 5) && browseHas(&browseAgain, "Harness Again"), "and is found");
    DNSServiceRefDeallocate(browseAgainRef);

    // ---- an idle loop: nothing scheduled soon, so only a wake-up gets a call from another thread noticed.
    // The responder announces the host's own records at 1, 2, 4, 8, 16 s... after a start; 20 s in, the next
    // timer is more than 10 s away. Without the wake-up pipe the checks below take that long.
    double quietUntil = loopStartedAt + 20;
    if (now() < quietUntil) usleep((useconds_t)((quietUntil - now()) * 1e6));
    DNSServiceRef idleRef = NULL;
    Query idle;
    double asked = now();
    check(startQuery(&idleRef, &idle, TYPE "." DOMAIN, kDNSServiceType_PTR) == kDNSServiceErr_NoError && waitFor(&idle.event, 15), "a question asked while the loop is idle is answered");
    snprintf(line, sizeof(line), "within 500 ms (%.0f ms)", (now() - asked) * 1000);
    check(now() - asked < 0.5, line);
    DNSServiceRefDeallocate(idleRef);
    DNSServiceRefDeallocate(again);
    stopSeconds = stopLoopAndJoin();
    snprintf(line, sizeof(line), "the idle loop stopped within 500 ms of being asked to (%.0f ms)", stopSeconds * 1000);
    check(stopSeconds < 0.5, line);

    TXTRecordDeallocate(&txt);
    check(callbacksElsewhere == 0, "callbacks ran on the loop thread or the calling thread, never elsewhere");
    printf("     (%d on the loop thread, %d on the calling thread)\n", callbacksOnLoopThread, callbacksOnMainThread);

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
