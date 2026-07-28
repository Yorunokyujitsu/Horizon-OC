#include <atomic>
#include <borealis.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <switch.h>
#include <unistd.h>

#include "bench.h"
#include "cpu_stress.h"
#include "gpu_bw.h"
#include "gpu_stress.h"
#include "hoc_clk.h"
#include "mt_cpu.h"
#include "mt_gpu.h"
#include "run_furmark.h"
}

static std::string fstr(const char *f, double v) {
    char b[64];
    std::snprintf(b, sizeof(b), f, v);
    return b;
}
static std::string fstru(const char *f, unsigned long long v) {
    char b[64];
    std::snprintf(b, sizeof(b), f, v);
    return b;
}

static brls::Label *makeRow(brls::Box *parent, const std::string &name) {
    auto *row = new brls::Box(brls::Axis::ROW);
    row->setMarginBottom(8.0f);
    auto *n = new brls::Label();
    n->setText(name);
    n->setGrow(1.0f);
    auto *v = new brls::Label();
    v->setText("-");
    row->addView(n);
    row->addView(v);
    parent->addView(row);
    return v;
}

struct StatCells {
    brls::Label *load = nullptr, *clock = nullptr, *volt = nullptr, *vddq = nullptr, *temp = nullptr;
};

static brls::Label *statCell(brls::Box *row, float fs) {
    auto *l = new brls::Label();
    if (fs > 0.0f)
        l->setFontSize(fs);
    row->addView(l);
    return l;
}
static void statSep(brls::Box *row, float fs) {
    auto *s = new brls::Label();
    s->setText("|");
    if (fs > 0.0f)
        s->setFontSize(fs);
    s->setTextColor(nvgRGB(120, 120, 120));
    s->setMarginLeft(7.0f);
    s->setMarginRight(7.0f);
    row->addView(s);
}
static void fmtLoad(brls::Label *l, unsigned val, bool isRam) {
    char b[32];
    if (isRam)
        std::snprintf(b, sizeof b, "%u.%u GB/s", val / 1000u, (val % 1000u) / 100u);
    else
        std::snprintf(b, sizeof b, "%u%%", val);
    l->setText(b);
}
static void fmtClock1(brls::Label *l, uint32_t hz) {
    char b[32];
    std::snprintf(b, sizeof b, "%u.%u MHz", hz / 1000000u, (hz / 100000u) % 10u);
    l->setText(b);
}
static void fmtVolt(brls::Label *l, uint32_t uv) {
    char b[32];
    std::snprintf(b, sizeof b, "%u mV", uv / 1000u);
    l->setText(b);
}
static void fmtTemp(brls::Label *l, int32_t mc) {
    char b[32];
    std::snprintf(b, sizeof b, "%d°C", mc / 1000);
    l->setText(b);
}

class SysInfoTab : public brls::Box {
    public:
    SysInfoTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *clk = new brls::Header();
        clk->setTitle("클럭");
        this->addView(clk);

        auto* s1 = new brls::Box();
        s1->setHeight(30.0f);
        this->addView(s1);

        cpuR = makeCompRow(this, "CPU");
        gpuR = makeCompRow(this, "GPU");
        ramR = makeRamRow(this);

        auto* s2 = new brls::Box();
        s2->setHeight(40.0f);
        this->addView(s2);

        auto *sys = new brls::Header();
        sys->setTitle("시스템");
        this->addView(sys);

        auto* s3 = new brls::Box();
        s3->setHeight(30.0f);
        this->addView(s3);

        mode = makeRow(this, "모드");
        threads = makeRow(this, "스레드");

        hocclk_init();
        refresh();
    }

    void frame(brls::FrameContext *ctx) override {
        if (++tick >= 15) {
            tick = 0;
            refresh();
        }
        brls::Box::frame(ctx);
    }

    private:
    static StatCells makeCompRow(brls::Box *parent, const char *name) {
        auto *row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(10.0f);
        auto *n = new brls::Label();
        n->setText(name);
        n->setGrow(1.0f);
        row->addView(n);
        StatCells c;
        c.load = statCell(row, 0);
        statSep(row, 0);
        c.clock = statCell(row, 0);
        statSep(row, 0);
        c.volt = statCell(row, 0);
        statSep(row, 0);
        c.temp = statCell(row, 0);
        parent->addView(row);
        return c;
    }
    static StatCells makeRamRow(brls::Box *parent) {
        auto *row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(10.0f);
        auto *n = new brls::Label();
        n->setText("RAM");
        n->setGrow(1.0f);
        row->addView(n);
        StatCells c;
        c.load = statCell(row, 0);
        statSep(row, 0);
        c.clock = statCell(row, 0);
        statSep(row, 0);
        c.volt = statCell(row, 0);
        statSep(row, 0);
        c.vddq = statCell(row, 0);
        statSep(row, 0);
        c.temp = statCell(row, 0);
        parent->addView(row);
        return c;
    }
    void setRow(StatCells &r, unsigned loadOrBw, bool isRam, uint32_t hz, uint32_t uv, int32_t mc) {
        fmtLoad(r.load, loadOrBw, isRam);
        fmtClock1(r.clock, hz);
        fmtVolt(r.volt, uv);
        fmtTemp(r.temp, mc);
    }
    void naRow(StatCells &r) {
        r.load->setText("N/A");
        r.clock->setText("-");
        r.volt->setText("-");
        r.temp->setText("-");
    }
    void refresh() {
        sysinfo_t s;
        bench_get_sysinfo(&s);
        mode->setText(s.is_4gb ? "애플리케이션" : "애플릿");
        threads->setText(fstru("%llu", (unsigned long long)s.threads));

        HocClkContext c;
        if (hocclk_get(&c)) {
            setRow(cpuR, c.stable.partLoad[3] / 10, false, c.stable.freqs[0], c.stable.voltages[2], c.stable.temps[5]);
            setRow(gpuR, c.stable.partLoad[2] / 10, false, c.stable.freqs[1], c.stable.voltages[3], c.stable.temps[6]);
            setRow(ramR, c.stable.partLoad[6], true, c.stable.freqs[2], c.stable.voltages[1], c.stable.temps[7]);
            fmtVolt(ramR.vddq, c.stable.voltages[4]);
        } else {
            naRow(cpuR);
            naRow(gpuR);
            naRow(ramR);
            ramR.vddq->setText("-");
        }
    }
    StatCells cpuR, gpuR, ramR;
    brls::Label *mode, *threads;
    int tick = 0;
};

class BenchTab : public brls::Box {
    public:
    BenchTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *t = new brls::Header();
        t->setTitle("벤치마크");
        t->setMarginBottom(10.0f);
        this->addView(t);

        runBtn = new brls::Button();
        runBtn->setText("실행");
        runBtn->registerClickAction([this](brls::View *) {
            start();
            return true;
        });
        this->addView(runBtn);

        status = new brls::Label();
        status->setText("유휴 상태");
        status->setMarginTop(14.0f);
        status->setMarginBottom(8.0f);
        this->addView(status);

        bar = new brls::Box(brls::Axis::ROW);
        bar->setHeight(18.0f);
        bar->setWidthPercentage(100.0f);
        bar->setMarginBottom(14.0f);
        barFill = new brls::Rectangle();
        barFill->setColor(nvgRGB(0, 193, 210));
        barFill->setWidthPercentage(0.0f);
        barTrack = new brls::Rectangle();
        barTrack->setColor(nvgRGB(48, 48, 54));
        barTrack->setGrow(1.0f);
        bar->addView(barFill);
        bar->addView(barTrack);
        this->addView(bar);

        auto *h1 = new brls::Header();
        h1->setTitle("GPU  CPU 대역폭");
        h1->setMarginBottom(14.0f);
        this->addView(h1);
        gpuCopy = makeRow(this, "GPU 복사");
        gpuRead = makeRow(this, "GPU 읽기");
        gpuWrite = makeRow(this, "GPU 쓰기");

        auto* spacer = new brls::Box();
        spacer->setHeight(12.0f);
        this->addView(spacer);

        cpuCopy = makeRow(this, "CPU 복사");
        cpuRead = makeRow(this, "CPU 읽기");
        cpuWrite = makeRow(this, "CPU 쓰기");

        auto *h3 = new brls::Header();
        h3->setTitle("RAM 레이턴시");
        h3->setMarginBottom(14.0f);
        this->addView(h3);
        l2 = makeRow(this, "L2 캐시");
        ram = makeRow(this, "전체 RAM");
    }

    ~BenchTab() override {
        if (ctx)
            bench_end(ctx);
    }

    void frame(brls::FrameContext *fc) override {
        if (running) {
            if (primed) {

                primed = false;
            } else {
                const char *label = "";
                float frac = 0.0f;
                bool more = bench_step(ctx, &res, &label, &frac);
                setProgress(frac);
                if (more) {
                    status->setText(fstr("%.0f%%", frac * 100.0f) + "   " + label);
                    primed = true;
                } else {
                    showResults();
                    bench_end(ctx);
                    ctx = nullptr;
                    running = false;
                    status->setText("완료");
                    appletSetAutoSleepDisabled(false);
                }
            }
        }
        brls::Box::frame(fc);
    }

    private:
    void setProgress(float f) {
        barFill->setWidthPercentage(f * 100.0f);
    }

    void showResults() {
        gpuCopy->setText(fstr("%.1f MB/s", res.gpu_copy));
        gpuRead->setText(fstr("%.1f MB/s", res.gpu_read));
        gpuWrite->setText(fstr("%.1f MB/s", res.gpu_write));
        cpuCopy->setText(fstr("%.1f MB/s", res.cpu_copy));
        cpuRead->setText(fstr("%.1f MB/s", res.cpu_read));
        cpuWrite->setText(fstr("%.1f MB/s", res.cpu_write));
        l2->setText(fstr("%.1f ns", res.l2_ns));
        ram->setText(fstr("%.1f ns", res.ram_ns));
    }

    void start() {
        if (running)
            return;
        memset(&res, 0, sizeof(res));
        ctx = bench_begin();
        if (!ctx) {
            status->setText("메모리 부족");
            return;
        }
        running = true;
        primed = true;
        setProgress(0.0f);
        status->setText("벤치마크 실행 중...");
        appletSetAutoSleepDisabled(true);
    }

    bench_ctx *ctx = nullptr;
    bool running = false;
    bool primed = false;
    bench_results_t res{};
    brls::Button *runBtn;
    brls::Label *status;
    brls::Box *bar;
    brls::Rectangle *barFill, *barTrack;
    brls::Label *gpuCopy, *gpuRead, *gpuWrite, *cpuCopy, *cpuRead, *cpuWrite, *l2, *ram;
};

struct StressShared {
    std::atomic<bool> running{ false }, stop{ false };
    std::atomic<double> gflops{ 0.0 };
    std::atomic<uint64_t> dispatches{ 0 }, mismatches{ 0 };
    std::thread worker;
};

class StressTab : public brls::Box {
    public:
    StressTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *t = new brls::Header();
        t->setTitle("실행");
        this->addView(t);

        auto *hint = new brls::Label();
        hint->setText("오류 횟수가 1회 이상이면 시스템이 불안정한 상태입니다,  버튼을 눌러 실행하세요.");
        hint->setFontSize(15.0f);
        hint->setTextColor(nvgRGB(150, 150, 150));
        hint->setMarginTop(16.0f);
        hint->setMarginBottom(14.0f);
        this->addView(hint);

        toggle = new brls::Button();
        toggle->setText("GPU 부하 테스트 시작");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        toggle->setMarginBottom(40.0f);
        this->addView(toggle);

        auto *s = new brls::Header();
        s->setTitle("상태");
        this->addView(s);

        statusL = new brls::Label();
        statusL->setText("중지됨");
        statusL->setMarginTop(16.0f);
        statusL->setMarginBottom(40.0f);
        this->addView(statusL);

        auto *h = new brls::Header();
        h->setTitle("정보");
        h->setMarginBottom(16.0f);
        this->addView(h);
        gflops = makeRow(this, "GFLOPS");
        dispatches = makeRow(this, "작업 횟수");
        mismatches = makeRow(this, "오류 횟수");
    }

    ~StressTab() override {
        stopWorker();
    }

    void willDisappear(bool resetState = false) override {
        stopWorker();
        brls::Box::willDisappear(resetState);
    }

    void frame(brls::FrameContext *ctx) override {
        if (sh.running.load()) {
            gflops->setText(fstr("%.1f", sh.gflops.load()));
            dispatches->setText(fstru("%llu", (unsigned long long)sh.dispatches.load()));
            mismatches->setText(fstru("%llu", (unsigned long long)sh.mismatches.load()));
        }
        brls::Box::frame(ctx);
    }

    private:
    void onToggle() {
        if (sh.running.load())
            stopWorker();
        else
            startWorker();
    }
    void startWorker() {
        if (sh.worker.joinable())
            sh.worker.join();
        sh.stop.store(false);
        sh.running.store(true);
        toggle->setText("GPU 부하 테스트 중지");
        statusL->setText("실행 중...");
        sh.worker = std::thread([this] {
            appletSetAutoSleepDisabled(true);
            uint64_t totD = 0, totM = 0;
            while (!sh.stop.load()) {
                double g = 0;
                uint64_t d = 0, m = 0;
                if (!gpu_stress_run(&g, &d, &m))
                    break;
                totD += d;
                totM += m;
                sh.gflops.store(g);
                sh.dispatches.store(totD);
                sh.mismatches.store(totM);
            }
            appletSetAutoSleepDisabled(false);
            sh.running.store(false);
        });
    }
    void stopWorker() {
        sh.stop.store(true);
        if (sh.worker.joinable())
            sh.worker.join();
        gpu_stress_shutdown();
        sh.running.store(false);
        if (toggle)
            toggle->setText("GPU 부하 테스트 시작");
        if (statusL)
            statusL->setText("중지됨");
    }
    StressShared sh;
    brls::Button *toggle;
    brls::Label *statusL, *gflops, *dispatches, *mismatches;
};

class FurmarkTab : public brls::Box {
    public:
    FurmarkTab(int which, const char *desc) : which(which) {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *t = new brls::Header();
        t->setTitle("실행");
        this->addView(t);

        auto *hint = new brls::Label();
        std::string text = std::string(desc) + ",  버튼을 눌러 실행하세요.";
        hint->setText(text);
        hint->setFontSize(15.0f);
        hint->setTextColor(nvgRGB(150, 150, 150));
        hint->setMarginTop(16.0f);
        hint->setMarginBottom(14.0f);
        this->addView(hint);

        toggle = new brls::Button();
        toggle->setText("시작");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        toggle->setMarginBottom(40.0f);
        this->addView(toggle);

        auto *s = new brls::Header();
        s->setTitle("상태");
        this->addView(s);

        statusL = new brls::Label();
        statusL->setText("중지됨");
        statusL->setMarginTop(16.0f);
        statusL->setMarginBottom(40.0f);
        this->addView(statusL);

        auto *h = new brls::Header();
        h->setTitle("정보");
        h->setMarginBottom(16.0f);
        this->addView(h);
        if (which == 3) {
            gpuFpsL = makeRow(this, "GPU FPS");
            cpuFpsL = makeRow(this, "CPU FPS");
        } else {
            gpuFpsL = makeRow(this, "FPS");
        }
    }

    ~FurmarkTab() override {
        if (run_furmark_running())
            run_furmark_stop();
    }

    void willDisappear(bool resetState = false) override {
        if (run_furmark_running())
            run_furmark_stop();
        brls::Box::willDisappear(resetState);
    }

    void frame(brls::FrameContext *fc) override {
        bool r = run_furmark_running() != 0;
        if (r != shown) {
            shown = r;
            toggle->setText(r ? "중지" : "시작");
            statusL->setText(r ? "실행 중..." : "중지됨");
            if (!r) {
                gpuFpsL->setText("-");
                if (cpuFpsL)
                    cpuFpsL->setText("-");
            }
        }
        if (r) {
            gpuFpsL->setText(fstr("%.1f", run_furmark_fps()));
            if (cpuFpsL)
                cpuFpsL->setText(fstr("%.1f", run_furmark_cpu_fps()));
        }
        brls::Box::frame(fc);
    }

    private:
    void onToggle() {
        if (run_furmark_running())
            run_furmark_stop();
        else
            run_furmark_start(which);
    }
    int which;
    bool shown = false;
    brls::Button *toggle;
    brls::Label *statusL;
    brls::Label *gpuFpsL = nullptr;
    brls::Label *cpuFpsL = nullptr;
};

class MemtesterTab : public brls::Box {
    public:
    MemtesterTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *t = new brls::Header();
        t->setTitle("테스트");
        this->addView(t);

        auto *modeRow = new brls::Box(brls::Axis::ROW);
        modeRow->setMarginBottom(10.0f);
        auto *ml = new brls::Label();
        ml->setText("모드:ㅤ");
        ml->setGrow(1.0f);
        ml->setFontSize(18.0f);
        ml->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
        modeRow->addView(ml);
        modeVal = new brls::Label();
        modeVal->setText(modeName(mode));
        modeVal->setFontSize(18.0f);
        modeRow->addView(modeVal);
        modeRow->setMarginTop(-34.0f);
        this->addView(modeRow);

        auto *hint = new brls::Label();
        hint->setText(" 또는  버튼으로 메모리 테스트 모드를 선택하고,  버튼을 눌러 실행하세요.");
        hint->setFontSize(15.0f);
        hint->setTextColor(nvgRGB(150, 150, 150));
        hint->setMarginTop(16.0f);
        hint->setMarginBottom(14.0f);
        this->addView(hint);

        toggle = new brls::Button();
        toggle->setText("시작");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        toggle->setMarginBottom(20.0f);
        this->addView(toggle);

        this->registerAction("Prev mode", brls::ControllerButton::BUTTON_LB, [this](brls::View *) {
            cycle(-1);
            return true;
        });
        this->registerAction("Next mode", brls::ControllerButton::BUTTON_RB, [this](brls::View *) {
            cycle(1);
            return true;
        });

        auto *s = new brls::Header();
        s->setTitle("상태");
        this->addView(s);

        statusL = new brls::Label();
        statusL->setText("중지됨");
        statusL->setMarginTop(16.0f);
        statusL->setMarginBottom(8.0f);
        this->addView(statusL);

        bar = new brls::Box(brls::Axis::ROW);
        bar->setHeight(18.0f);
        bar->setWidthPercentage(100.0f);
        bar->setMarginBottom(14.0f);
        barFill = new brls::Rectangle();
        barFill->setColor(nvgRGB(0, 193, 210));
        barFill->setWidthPercentage(0.0f);
        barTrack = new brls::Rectangle();
        barTrack->setColor(nvgRGB(48, 48, 54));
        barTrack->setGrow(1.0f);
        bar->addView(barFill);
        bar->addView(barTrack);
        this->addView(bar);

        auto *h = new brls::Header();
        h->setTitle("정보");
        h->setMarginTop(20.0f);
        h->setMarginBottom(16.0f);
        this->addView(h);
        rowA = makeRow(this, "반복 횟수");
        rowB = makeRow(this, "오류 횟수");
        rowC = makeRow(this, "상세 정보");

        updateBar();
    }

    void setProgress(float f) {
        barFill->setWidthPercentage(f * 100.0f);
    }

    void updateBar() {
        bar->setVisibility(isGpu() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    }

    ~MemtesterTab() override {
        stopAny();
    }

    void willDisappear(bool resetState = false) override {
        stopAny();
        brls::Box::willDisappear(resetState);
    }

    void frame(brls::FrameContext *ctx) override {
        if (isGpu()) {
            mt_gpu_status_t s;
            mt_gpu_get(&s);
            if (s.running || lastRunning) {
                rowA->setText(fstru("%llu", (unsigned long long)s.loop));
                rowB->setText(fstru("%llu", (unsigned long long)s.mismatches));
                rowC->setText(fstru("%llu MB 검사 완료", (unsigned long long)s.size_mb));
                statusL->setText(s.error ? std::string("오류: ") + s.status : std::string(s.status));
            }
            syncToggle(s.running != 0);
        } else {
            mt_cpu_status_t s;
            mt_cpu_get(&s);
            if (mt_cpu_running() || lastRunning) {
                rowA->setText(fstru("%llu", (unsigned long long)s.loop));
                rowB->setText(fstru("%llu", (unsigned long long)s.mismatches));
                char d[96];
                if (mode == 1)
                    std::snprintf(d, sizeof d, "%llu MB, burn-in x%llu",
                                  (unsigned long long)s.total_mb, (unsigned long long)s.burnin_iters);
                else
                    std::snprintf(d, sizeof d, "%llu MB, %d 스레드",
                                  (unsigned long long)s.total_mb, s.threads);
                rowC->setText(d);
                setProgress(mt_cpu_running() ? s.progress : 0.0f);
                if (mt_cpu_running())
                    statusL->setText(std::string(s.mismatches ? "오류 발견! 실행 중 - " : "실행 중 - ") +
                                     (s.test ? s.test : ""));
            }
            syncToggle(mt_cpu_running() != 0);
        }
        brls::Box::frame(ctx);
    }

    private:
    static const char *modeName(int m) {
        switch (m) {
            case 0: return "CPU - 메모리 테스트";
            case 1: return "CPU - 메모리 테스트 + 대역폭 burn-in";
            case 2: return "GPU - 메모리 테스트 (빠른 검사)";
            case 3: return "GPU - 메모리 테스트 (전체 검사)";
        }
        return "";
    }
    bool isGpu() const {
        return mode >= 2;
    }
    bool anyRunning() {
        return mt_cpu_running() || mt_gpu_running();
    }
    void cycle(int dir) {
        if (anyRunning())
            return;
        mode = (mode + dir + 4) % 4;
        modeVal->setText(modeName(mode));
        updateBar();
    }
    void onToggle() {
        if (anyRunning())
            stopAny();
        else
            startAny();
    }
    void startAny() {
        rowA->setText("-");
        rowB->setText("-");
        rowC->setText("-");
        switch (mode) {
            case 0: mt_cpu_start(0); break;
            case 1: mt_cpu_start(1); break;
            case 2: mt_gpu_start(0); break;
            case 3: mt_gpu_start(1); break;
        }
    }
    void stopAny() {
        if (mt_cpu_running())
            mt_cpu_stop();
        if (mt_gpu_running())
            mt_gpu_stop();
    }
    void syncToggle(bool running) {
        if (running == lastRunning)
            return;
        lastRunning = running;
        toggle->setText(running ? "중지" : "시작");
        if (!running)
            statusL->setText("중지됨");
    }
    int mode = 0;
    bool lastRunning = false;
    brls::Label *modeVal, *statusL, *rowA, *rowB, *rowC;
    brls::Button *toggle;
    brls::Box *bar;
    brls::Rectangle *barFill, *barTrack;
};

class CpuStressTab : public brls::Box {
    public:
    CpuStressTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *t = new brls::Header();
        t->setTitle("실행");
        this->addView(t);

        auto *modeRow = new brls::Box(brls::Axis::ROW);
        modeRow->setMarginBottom(10.0f);
        auto *ml = new brls::Label();
        ml->setText("모드:ㅤ");
        ml->setGrow(1.0f);
        ml->setFontSize(18.0f);
        ml->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
        modeRow->addView(ml);
        modeVal = new brls::Label();
        modeVal->setText(modeName(mode));
        modeVal->setFontSize(18.0f);
        modeRow->addView(modeVal);
        modeRow->setMarginTop(-34.0f);
        this->addView(modeRow);

        auto *hint = new brls::Label();
        hint->setText(" 또는  버튼으로 CPU 부하 테스트 모드를 선택하고,  버튼을 눌러 실행하세요.");
        hint->setFontSize(15.0f);
        hint->setTextColor(nvgRGB(150, 150, 150));
        hint->setMarginTop(16.0f);
        hint->setMarginBottom(14.0f);
        this->addView(hint);

        toggle = new brls::Button();
        toggle->setText("시작");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        toggle->setMarginBottom(40.0f);
        this->addView(toggle);

        this->registerAction("Prev mode", brls::ControllerButton::BUTTON_LB, [this](brls::View *) {
            cycle(-1);
            return true;
        });
        this->registerAction("Next mode", brls::ControllerButton::BUTTON_RB, [this](brls::View *) {
            cycle(1);
            return true;
        });

        auto *s = new brls::Header();
        s->setTitle("상태");
        this->addView(s);

        statusL = new brls::Label();
        statusL->setText("중지됨");
        statusL->setMarginTop(16.0f);
        statusL->setMarginBottom(40.0f);
        this->addView(statusL);

        auto *h = new brls::Header();
        h->setTitle("정보");
        h->setMarginBottom(16.0f);
        this->addView(h);
        rowA = makeRow(this, "반복 횟수");
        rowB = makeRow(this, "오류 횟수");
        rowC = makeRow(this, "스레드");
    }

    ~CpuStressTab() override {
        if (cpu_stress_running())
            cpu_stress_stop();
    }

    void willDisappear(bool resetState = false) override {
        if (cpu_stress_running())
            cpu_stress_stop();
        brls::Box::willDisappear(resetState);
    }

    void frame(brls::FrameContext *ctx) override {
        cpu_stress_status_t s;
        cpu_stress_get(&s);
        if (cpu_stress_running() || lastRunning) {
            rowA->setText(fstru("%llu", (unsigned long long)s.iters));
            rowB->setText(fstru("%llu", (unsigned long long)s.mismatches));
            rowC->setText(fstru("%llu", (unsigned long long)s.threads));
            if (cpu_stress_running())
                statusL->setText(s.mismatches ? "오류 발견! 실행 중..." : "실행 중...");
        }
        syncToggle(cpu_stress_running() != 0);
        brls::Box::frame(ctx);
    }

    private:
    static const char *modeName(int m) {
        switch (m) {
            case 0: return "행렬 연산";
            case 1: return "하노이 테스트 (검증)";
        }
        return "";
    }
    void cycle(int dir) {
        if (cpu_stress_running())
            return;
        mode = (mode + dir + 2) % 2;
        modeVal->setText(modeName(mode));
    }
    void onToggle() {
        if (cpu_stress_running())
            cpu_stress_stop();
        else {
            rowA->setText("-");
            rowB->setText("-");
            rowC->setText("-");
            cpu_stress_start(mode);
        }
    }
    void syncToggle(bool running) {
        if (running == lastRunning)
            return;
        lastRunning = running;
        toggle->setText(running ? "중지" : "시작");
        if (!running)
            statusL->setText("중지됨");
    }
    int mode = 0;
    bool lastRunning = false;
    brls::Label *modeVal, *statusL, *rowA, *rowB, *rowC;
    brls::Button *toggle;
};

class CreditsTab : public brls::Box {
    public:
    CreditsTab() {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setPadding(40.0f, 60.0f, 40.0f, 60.0f);

        auto *title = new brls::Label();
        title->setText("Benchmark Toolbox");
        title->setFontSize(26.0f);
        this->addView(title);

        auto *by = new brls::Label();
        by->setText("개발자: Souldbminer, Lightos_  GPLv2 라이센스에 따라 제작되었습니다.");
        by->setFontSize(16.0f);
        by->setTextColor(nvgRGB(150, 150, 150));
        this->addView(by);

        auto* s1 = new brls::Box();
        s1->setHeight(20.0f);
        this->addView(s1);

        auto *h = new brls::Header();
        h->setTitle("크레딧");
        this->addView(h);

        auto* s2 = new brls::Box();
        s2->setHeight(20.0f);
        this->addView(s2);

        makeRow(this, "Memtester")->setText("Simon Kirby  Charles Cazabon  KazushiMe  CTCaer");
        makeRow(this, "FurMark")->setText("StanislavPetrovV  AnxietyTimmy");
        makeRow(this, "GPU Test")->setText("NaGaa95");
        makeRow(this, "Membench")->setText("Siarhei Siamashka  KazushiMe  Lineon");
        makeRow(this, "Stress-ng")->setText("ColinIanKing  Lineon");

        auto* s3 = new brls::Box();
        s3->setHeight(20.0f);
        this->addView(s3);

        auto *tr = new brls::Header();
        tr->setTitle("번역");
        this->addView(tr);

        auto* s4 = new brls::Box();
        s4->setHeight(20.0f);
        this->addView(s4);

        makeRow(this, "한국어")->setText("Yorunokyujitsu");

        auto *note = new brls::Label();
        note->setText("모든 원 개발자 여러분께 감사드립니다.");
        note->setFontSize(15.0f);
        note->setTextColor(nvgRGB(150, 150, 150));
        note->setMarginTop(18.0f);
        this->addView(note);
    }
};

class AppFrame : public brls::TabFrame {
    public:
    AppFrame() {
        box = dynamic_cast<brls::Box *>(this->getView("brls/applet_frame/header_stats"));
        if (box) {
            box->setJustifyContent(brls::JustifyContent::FLEX_END);
            box->setAlignItems(brls::AlignItems::CENTER);
            const float fs = 13.0f;
            for (int i = 0; i < 3; i++) {
                grp[i].load = statCell(box, fs);
                if (i)
                    grp[i].load->setMarginLeft(18.0f);
                statSep(box, fs);
                grp[i].clock = statCell(box, fs);
                statSep(box, fs);
                grp[i].temp = statCell(box, fs);
            }
        }
        hocclk_init();
        update();
    }

    void frame(brls::FrameContext *ctx) override {
        if (box && ++tick >= 12) {
            tick = 0;
            update();
        }
        brls::TabFrame::frame(ctx);
    }

    private:
    void setGrp(StatCells &g, const char *name, unsigned loadOrBw, bool isRam, uint32_t hz, int32_t mc) {
        char b[48];
        if (isRam)
            std::snprintf(b, sizeof b, "%s %u.%u GB/s", name, loadOrBw / 1000u, (loadOrBw % 1000u) / 100u);
        else
            std::snprintf(b, sizeof b, "%s %u%%", name, loadOrBw);
        g.load->setText(b);
        fmtClock1(g.clock, hz);
        fmtTemp(g.temp, mc);
    }
    void update() {
        if (!box)
            return;
        HocClkContext c;
        if (!hocclk_get(&c)) {
            grp[0].load->setText("hoc:clk N/A");
            grp[0].clock->setText("-");
            grp[0].temp->setText("-");
            grp[1].load->setText("-");
            grp[1].clock->setText("-");
            grp[1].temp->setText("-");
            grp[2].load->setText("-");
            grp[2].clock->setText("-");
            grp[2].temp->setText("-");
            return;
        }
        setGrp(grp[0], "CPU", c.stable.partLoad[3] / 10, false, c.stable.realFreqs[0], c.stable.temps[5]);
        setGrp(grp[1], "GPU", c.stable.partLoad[2] / 10, false, c.stable.realFreqs[1], c.stable.temps[6]);
        setGrp(grp[2], "RAM", c.stable.partLoad[6], true, c.stable.realFreqs[2], c.stable.temps[7]);
    }
    brls::Box *box = nullptr;
    StatCells grp[3];
    int tick = 0;
};

class MainActivity : public brls::Activity {
    public:
    brls::View *createContentView() override {
        auto *tab = new AppFrame();
        tab->setTitle("Benchmark Toolbox");
        tab->setIconFromRes("img/logo.png");
        tab->addHeader("정보");
        tab->addSeparator();
        tab->addTab("애플리케이션", [] { return new CreditsTab(); });
        tab->addTab("시스템", [] { return new SysInfoTab(); });

        tab->addHeader("종합");
        tab->addSeparator();
        tab->addTab("블랙홀", [] { return new FurmarkTab(3, "CPU  GPU 동시 최대 부하 테스트"); });

        tab->addHeader("CPU");
        tab->addSeparator();
        tab->addTab("부하 테스트", [] { return new CpuStressTab(); });
        tab->addTab("레이 트레이싱", [] { return new FurmarkTab(4, "CPU 레이 트레이서"); });

        tab->addHeader("GPU");
        tab->addSeparator();
        tab->addTab("부하 테스트", [] { return new StressTab(); });
        tab->addTab("Furmark", [] { return new FurmarkTab(0, "Switch용 FurMark (48 스텝)"); });
        tab->addTab("레이 트레이싱", [] { return new FurmarkTab(2, "GPU 레이 트레이서"); });

        tab->addHeader("RAM");
        tab->addSeparator();
        tab->addTab("메모리 테스트", [] { return new MemtesterTab(); });
        tab->addTab("벤치마크", [] { return new BenchTab(); });
        tab->addTab("Furmark", [] { return new FurmarkTab(1, "RAM 부하를 추가한 FurMark"); });
        tab->addTab("CPU 레이 트레이싱", [] { return new FurmarkTab(5, "RAM 부하를 추가한 CPU 레이 트레이싱"); });
        return tab;
    }
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    brls::Logger::setLogLevel(brls::LogLevel::INFO);

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("Benchmark Toolbox");
    brls::Application::setGlobalQuit(true);
    brls::Application::pushActivity(new MainActivity());

    while (brls::Application::mainLoop())
        ;

    // Global quit (+) can return from the main loop without destroying the
    // activity, leaving background workers (and their threads / large RAM
    // allocations) live across _exit. Tear them down first.
    if (run_furmark_running())
        run_furmark_stop();
    if (mt_gpu_running())
        mt_gpu_stop();
    if (mt_cpu_running())
        mt_cpu_stop();
    if (cpu_stress_running())
        cpu_stress_stop();

    _exit(EXIT_SUCCESS);
}
