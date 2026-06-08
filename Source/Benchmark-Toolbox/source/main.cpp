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
#include "gpu_bw.h"
#include "gpu_stress.h"
#include "hoc_clk.h"
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
    brls::Label *load = nullptr, *clock = nullptr, *volt = nullptr, *temp = nullptr;
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
static void fmtClock0(brls::Label *l, uint32_t hz) {
    char b[32];
    std::snprintf(b, sizeof b, "%u MHz", hz / 1000000u);
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
        clk->setTitle("Clocks");
        this->addView(clk);
        cpuR = makeCompRow(this, "CPU");
        gpuR = makeCompRow(this, "GPU");
        ramR = makeCompRow(this, "RAM");

        auto *sys = new brls::Header();
        sys->setTitle("System");
        this->addView(sys);
        mode = makeRow(this, "Mode");
        threads = makeRow(this, "Threads");

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
        mode->setText(s.is_4gb ? "Application" : "Applet");
        threads->setText(fstru("%llu", (unsigned long long)s.threads));

        HocClkContext c;
        if (hocclk_get(&c)) {
            setRow(cpuR, c.stable.partLoad[3] / 10, false, c.stable.freqs[0], c.stable.voltages[2], c.stable.temps[5]);
            setRow(gpuR, c.stable.partLoad[2] / 10, false, c.stable.freqs[1], c.stable.voltages[3], c.stable.temps[6]);
            setRow(ramR, c.stable.partLoad[6], true, c.stable.freqs[2], c.stable.voltages[1], c.stable.temps[7]);
        } else {
            naRow(cpuR);
            naRow(gpuR);
            naRow(ramR);
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

        runBtn = new brls::Button();
        runBtn->setText("Run");
        runBtn->registerClickAction([this](brls::View *) {
            start();
            return true;
        });
        this->addView(runBtn);

        status = new brls::Label();
        status->setText("Idle");
        status->setMarginTop(8.0f);
        status->setMarginBottom(6.0f);
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
        h1->setTitle("GPU bandwidth");
        this->addView(h1);
        gpuCopy = makeRow(this, "GPU Copy");
        gpuRead = makeRow(this, "GPU Read");
        gpuWrite = makeRow(this, "GPU Write");

        auto *h2 = new brls::Header();
        h2->setTitle("CPU bandwidth");
        this->addView(h2);
        cpuCopy = makeRow(this, "CPU Copy");
        cpuRead = makeRow(this, "CPU Read");
        cpuWrite = makeRow(this, "CPU Write");

        auto *h3 = new brls::Header();
        h3->setTitle("RAM latency");
        this->addView(h3);
        l2 = makeRow(this, "L2");
        ram = makeRow(this, "Full RAM");
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
                    status->setText("Done");
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
            status->setText("Out of memory");
            return;
        }
        running = true;
        primed = true;
        setProgress(0.0f);
        status->setText("Running benchmark...");
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

        toggle = new brls::Button();
        toggle->setText("Start GPU stress");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        this->addView(toggle);

        statusL = new brls::Label();
        statusL->setText("Stopped");
        statusL->setMarginTop(8.0f);
        statusL->setMarginBottom(8.0f);
        this->addView(statusL);

        auto *h = new brls::Header();
        h->setTitle("Live");
        this->addView(h);
        gflops = makeRow(this, "GFLOPS");
        dispatches = makeRow(this, "Dispatches");
        mismatches = makeRow(this, "Mismatches");

        auto *info = new brls::Label();
        info->setText("GPU stress test, Mismatches > 0 indicate instability.");
        info->setMarginTop(16.0f);
        this->addView(info);
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
        toggle->setText("Stop GPU stress");
        statusL->setText("Running...");
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
            toggle->setText("Start GPU stress");
        if (statusL)
            statusL->setText("Stopped");
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

        toggle = new brls::Button();
        toggle->setText("Start");
        toggle->registerClickAction([this](brls::View *) {
            onToggle();
            return true;
        });
        this->addView(toggle);

        statusL = new brls::Label();
        statusL->setText("Stopped");
        statusL->setMarginTop(8.0f);
        statusL->setMarginBottom(12.0f);
        this->addView(statusL);

        auto *info = new brls::Label();
        info->setText(desc);
        this->addView(info);
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
            toggle->setText(r ? "Stop" : "Start");
            statusL->setText(r ? "Running..." : "Stopped");
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
        fmtClock0(g.clock, hz);
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
        setGrp(grp[0], "CPU", c.stable.partLoad[3] / 10, false, c.stable.freqs[0], c.stable.temps[5]);
        setGrp(grp[1], "GPU", c.stable.partLoad[2] / 10, false, c.stable.freqs[1], c.stable.temps[6]);
        setGrp(grp[2], "RAM", c.stable.partLoad[6], true, c.stable.freqs[2], c.stable.temps[7]);
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
        tab->addTab("System Info", [] { return new SysInfoTab(); });
        tab->addSeparator();
        tab->addTab("Membench", [] { return new BenchTab(); });
        tab->addTab("GPU Test", [] { return new StressTab(); });
        tab->addSeparator();
        tab->addTab("Furmark", [] { return new FurmarkTab(0, "FurMark for Switch (48 step)"); });
        tab->addTab("Furmark RAM", [] { return new FurmarkTab(1, "FurMark with extra ram stress"); });
        tab->addTab("GPU Path Trace", [] { return new FurmarkTab(2, "GPU Path Tracer"); });
        tab->addTab("Black Hole", [] { return new FurmarkTab(3, "CPU+GPU black-hole."); });
        tab->addTab("CPU Ray Trace", [] { return new FurmarkTab(4, "CPU Path Tracer"); });
        tab->addTab("CPU RAM", [] { return new FurmarkTab(5, "CPU RT with extra RAM stress"); });
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

    _exit(EXIT_SUCCESS);
}
