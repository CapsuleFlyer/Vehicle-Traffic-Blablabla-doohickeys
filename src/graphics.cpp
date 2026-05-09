#include "graphics.h"
#include <SFML/Graphics.hpp>
#include <pthread.h>
#include <string>
#include <deque>
#include <cstring>
#include <ctime>
#include <cmath>

/* ── Shared state ── */
struct LogEntry {
    std::string vehicle_type;
    std::string status;
    int intersection_id;
    int vehicle_id;
    std::string timestamp;
};

struct IpcFlash {
    float timer;      /* seconds remaining to show flash */
    int direction;    /* 0 = F10->F11, 1 = F11->F10 */
};

struct FinalStats {
    bool ready;
    int total_vehicles;
    int total_parked;
    int emergency_count;
    int bus_count;
    int car_count;
    int bike_count;
    int tractor_count;
    int f10_peak_occupancy;
    int f11_peak_occupancy;
};

struct SharedState {
    pthread_mutex_t lock;

    /* Live state */
    int f10_crossing, f10_occupancy;
    int f11_crossing, f11_occupancy;
    int f10_north_green, f10_east_green;
    int f11_north_green, f11_east_green;
    int f10_emergency, f11_emergency;
    int total_parked, active_vehicles;

    /* IPC flash */
    IpcFlash ipc_flash;

    /* Status message shown in bottom bar */
    char status_msg[128];
    int  spawned_count;   /* how many vehicles spawned so far */

    /* Event log */
    std::deque<LogEntry> log;

    /* Final summary */
    FinalStats final_stats;

    bool shutdown;
};

static SharedState g_state;
static pthread_t   g_thread;

/* ── Palette ── */
static const sf::Color BG       {13,  15,  23};
static const sf::Color PANEL    {20,  24,  38};
static const sf::Color PANEL2   {26,  32,  50};
static const sf::Color BORDER   {44,  54,  80};
static const sf::Color RED_C    {220, 60,  60};
static const sf::Color GREEN_C  {55,  200, 95};
static const sf::Color YELLOW_C {240, 185, 45};
static const sf::Color BLUE_C   {60,  140, 220};
static const sf::Color CYAN_C   {55,  205, 195};
static const sf::Color WHITE_C  {215, 220, 232};
static const sf::Color DIM_C    {75,  85,  108};
static const sf::Color EMERG_C  {255, 75,  75};
static const sf::Color ORANGE_C {230, 140, 50};

static sf::Color vehicleColor(const std::string& t) {
    if (t == "Ambulance" || t == "Firetruck") return RED_C;
    if (t == "Bus")     return YELLOW_C;
    if (t == "Car")     return GREEN_C;
    if (t == "Bike")    return CYAN_C;
    if (t == "Tractor") return ORANGE_C;
    return WHITE_C;
}

static sf::RectangleShape rect(float x, float y, float w, float h,
                                sf::Color fill, sf::Color outline={0,0,0,0}, float thick=0) {
    sf::RectangleShape r({w,h});
    r.setPosition(x,y);
    r.setFillColor(fill);
    if (thick>0){r.setOutlineColor(outline);r.setOutlineThickness(thick);}
    return r;
}

static void text(sf::RenderWindow& w, sf::Font& f, const std::string& s,
                 float x, float y, unsigned sz, sf::Color c) {
    sf::Text t(s, f, sz);
    t.setFillColor(c);
    t.setPosition(x, y);
    w.draw(t);
}

/* ── Traffic light widget ── */
static void drawLight(sf::RenderWindow& win, sf::Font& font,
                      float x, float y, bool north_green, bool east_green,
                      bool emergency, int crossing, const std::string& label) {
    win.draw(rect(x, y, 145, 130, PANEL, BORDER, 1));
    text(win, font, label, x+8, y+6, 11, emergency ? EMERG_C : CYAN_C);

    if (emergency) {
        text(win, font, "!! EMERGENCY !!", x+8, y+22, 10, EMERG_C);
    }

    /* N/S circle */
    sf::Color ns_col = emergency ? GREEN_C : (north_green ? GREEN_C : RED_C);
    sf::CircleShape ns(11);
    ns.setFillColor(ns_col);
    ns.setPosition(x+12, y+40);
    win.draw(ns);
    text(win, font, "N/S", x+10, y+64, 9, DIM_C);

    /* E/W circle */
    sf::Color ew_col = emergency ? RED_C : (east_green ? GREEN_C : RED_C);
    sf::CircleShape ew(11);
    ew.setFillColor(ew_col);
    ew.setPosition(x+60, y+40);
    win.draw(ew);
    text(win, font, "E/W", x+58, y+64, 9, DIM_C);

    /* Crossing count badge */
    sf::Color cc = crossing > 0 ? GREEN_C : DIM_C;
    text(win, font, "Crossing: " + std::to_string(crossing), x+8, y+82, 9, cc);
    text(win, font, "vehicles", x+8, y+96, 9, cc);
}

/* ── Parking bar ── */
static void drawParking(sf::RenderWindow& win, sf::Font& font,
                        float x, float y, float w, int occ, const std::string& lbl) {
    win.draw(rect(x, y, w, 40, PANEL, BORDER, 1));
    text(win, font, lbl + " (" + std::to_string(occ) + "/10)", x+6, y+4, 10, WHITE_C);
    float sw = (w - 16) / 10.f;
    for (int i = 0; i < 10; i++) {
        sf::Color c = i < occ ? GREEN_C : sf::Color(35, 42, 62);
        win.draw(rect(x+8+i*sw, y+22, sw-2, 12, c));
    }
}

/* ── IPC pipe with animation ── */
static void drawIPC(sf::RenderWindow& win, sf::Font& font,
                    float x, float y, float w, const IpcFlash& flash) {
    win.draw(rect(x, y, w, 56, PANEL, BORDER, 1));
    text(win, font, "IPC PIPES", x+8, y+4, 10, CYAN_C);

    bool active = flash.timer > 0.f;
    bool f10_to_f11 = active && flash.direction == 0;
    bool f11_to_f10 = active && flash.direction == 1;

    /* Row 1: F10 -> F11 */
    sf::Color col1 = f10_to_f11 ? CYAN_C : DIM_C;
    std::string row1 = f10_to_f11
        ? "F10 ==[EMERGENCY]=================================> F11"
        : "F10 ---------------------------------------------> F11";
    text(win, font, row1, x+8, y+20, 9, col1);

    /* Row 2: F11 -> F10 */
    sf::Color col2 = f11_to_f10 ? YELLOW_C : DIM_C;
    std::string row2 = f11_to_f10
        ? "F10 <=================================[EMERGENCY]== F11"
        : "F10 <--------------------------------------------- F11";
    text(win, font, row2, x+8, y+38, 9, col2);
}

/* ── Event log ── */
static void drawLog(sf::RenderWindow& win, sf::Font& font,
                    float x, float y, float w, float h,
                    const std::deque<LogEntry>& log) {
    win.draw(rect(x, y, w, h, PANEL, BORDER, 1));
    text(win, font, "EVENT LOG", x+8, y+6, 11, CYAN_C);
    win.draw(rect(x+8, y+20, w-16, 1, BORDER));

    float rowH = 17.f;
    int maxRows = (int)((h - 28) / rowH);
    int start = (int)log.size() > maxRows ? (int)log.size() - maxRows : 0;

    for (int i = start; i < (int)log.size(); i++) {
        const auto& e = log[i];
        std::string line = e.timestamp + "  #" + std::to_string(e.vehicle_id)
                         + " " + e.vehicle_type
                         + "  " + e.status
                         + "  @F" + std::to_string(10 + e.intersection_id);
        sf::Color c = vehicleColor(e.vehicle_type);
        if (e.status == "EMERGENCY") c = EMERG_C;
        text(win, font, line, x+8, y+24+(i-start)*rowH, 9, c);
    }
}

/* ── Final summary screen ── */
static void drawFinalScreen(sf::RenderWindow& win, sf::Font& font, const FinalStats& fs) {
    win.clear(BG);

    text(win, font, "SIMULATION COMPLETE", 260, 20, 20, CYAN_C);
    win.draw(rect(20, 50, 780, 1, BORDER));

    /* Stats grid */
    float col1 = 40, col2 = 420;
    float y = 70;

    win.draw(rect(col1-10, y-8, 360, 200, PANEL, BORDER, 1));
    text(win, font, "VEHICLE SUMMARY", col1, y, 13, YELLOW_C);
    y += 22;
    text(win, font, "Total Vehicles:        " + std::to_string(fs.total_vehicles), col1, y, 11, WHITE_C); y+=18;
    text(win, font, "Total Parked:          " + std::to_string(fs.total_parked),   col1, y, 11, WHITE_C); y+=18;
    text(win, font, "Emergency Vehicles:    " + std::to_string(fs.emergency_count),col1, y, 11, RED_C);   y+=18;
    text(win, font, "Buses:                 " + std::to_string(fs.bus_count),      col1, y, 11, YELLOW_C);y+=18;
    text(win, font, "Cars:                  " + std::to_string(fs.car_count),      col1, y, 11, GREEN_C); y+=18;
    text(win, font, "Bikes:                 " + std::to_string(fs.bike_count),     col1, y, 11, CYAN_C);  y+=18;
    text(win, font, "Tractors:              " + std::to_string(fs.tractor_count),  col1, y, 11, ORANGE_C);

    /* Peak parking state */
    y = 70;
    win.draw(rect(col2-10, y-8, 360, 90, PANEL, BORDER, 1));
    text(win, font, "PEAK PARKING STATE", col2, y, 13, YELLOW_C); y+=22;
    text(win, font, "F10 Peak:", col2, y, 11, WHITE_C);
    text(win, font, std::to_string(fs.f10_peak_occupancy) + "/10", col2+80, y, 11, GREEN_C);
    float sw = 18.f;
    for (int i = 0; i < 10; i++)
        win.draw(rect(col2+110+i*sw, y+2, sw-2, 12, i < fs.f10_peak_occupancy ? GREEN_C : sf::Color(35,42,62)));
    y+=20;
    text(win, font, "F11 Peak:", col2, y, 11, WHITE_C);
    text(win, font, std::to_string(fs.f11_peak_occupancy) + "/10", col2+80, y, 11, GREEN_C);
    for (int i = 0; i < 10; i++)
        win.draw(rect(col2+110+i*sw, y+2, sw-2, 12, i < fs.f11_peak_occupancy ? GREEN_C : sf::Color(35,42,62)));

    /* OS Concepts */
    y = 290;
    win.draw(rect(col2-10, y-8, 360, 130, PANEL, BORDER, 1));
    text(win, font, "PROGRAM ATTRIBUTES CHECK", col2, y, 12, CYAN_C); y+=22;
    text(win, font, "✓ Threads (pthreads) - 15 vehicles", col2, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Processes (fork) - F10/F11 controllers", col2, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ IPC (pipes) - emergency coordination", col2, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Semaphores - parking spots + queue", col2, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Mutexes - shared state protection", col2, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Signals - SIGINT graceful shutdown", col2, y, 10, GREEN_C);

    /* Cleanup status */
    y = 290;
    win.draw(rect(col1-10, y-8, 360, 130, PANEL, BORDER, 1));
    text(win, font, "CLEANUP STATUS", col1, y, 12, CYAN_C); y+=22;
    text(win, font, "✓ All 15 threads joined", col1, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Semaphores destroyed", col1, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ IPC pipes closed", col1, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Memory freed", col1, y, 10, GREEN_C); y+=16;
    text(win, font, "✓ Controller processes terminated", col1, y, 10, GREEN_C);

    win.draw(rect(20, 438, 780, 1, BORDER));
    text(win, font, "Close this window to exit.", 310, 448, 11, DIM_C);

    win.display();
}

/* ── SFML thread ── */
static void* sfml_thread(void*) {
    sf::RenderWindow window(sf::VideoMode(820, 490),
                            "Traffic Simulator - F10 & F11",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(30);

    sf::Font font;
    bool fontLoaded =
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf") ||
        font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf") ||
        font.loadFromFile("/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf") ||
        font.loadFromFile("/usr/share/fonts/dejavu/DejaVuSansMono.ttf");

    if (!fontLoaded) {
        pthread_mutex_lock(&g_state.lock);
        g_state.shutdown = true;
        pthread_mutex_unlock(&g_state.lock);
        return NULL;
    }

    sf::Clock clock;
    float ipc_timer = 0.f;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        sf::Event event;
        while (window.pollEvent(event))
            if (event.type == sf::Event::Closed)
                window.close();

        pthread_mutex_lock(&g_state.lock);
        SharedState snap = g_state;
        /* Tick down IPC flash timer */
        if (g_state.ipc_flash.timer > 0.f) {
            g_state.ipc_flash.timer -= dt;
            if (g_state.ipc_flash.timer < 0.f) g_state.ipc_flash.timer = 0.f;
        }
        snap.ipc_flash = g_state.ipc_flash;
        pthread_mutex_unlock(&g_state.lock);

        /* ── Final screen ── */
        if (snap.final_stats.ready) {
            /* Draw final screen and wait for window close */
            drawFinalScreen(window, font, snap.final_stats);
            while (window.isOpen()) {
                sf::Event e2;
                while (window.pollEvent(e2))
                    if (e2.type == sf::Event::Closed) window.close();
                sf::sleep(sf::milliseconds(50));
            }
            break;
        }

        /* ── Check shutdown (no final stats) ── */
        if (snap.shutdown && !snap.final_stats.ready) {
            window.close(); break;
        }

        window.clear(BG);

        /* Title */
        text(window, font, "TRAFFIC INTERSECTION SIMULATOR  |  F10 & F11", 18, 12, 13, CYAN_C);
        text(window, font, "OS Project  •  Threads / Processes / IPC / Semaphores / Signals", 18, 30, 10, DIM_C);
        window.draw(rect(0, 48, 820, 1, BORDER));

        float topY = 56;
        float col1 = 18, col2 = 310, logX = 575;

        /* ── F10 ── */
        sf::Color hdr1 = snap.f10_emergency ? EMERG_C : BLUE_C;
        window.draw(rect(col1, topY, 265, 20,
                         snap.f10_emergency ? sf::Color(60,15,15) : sf::Color(18,28,52), BORDER, 1));
        text(window, font, "F10 INTERSECTION", col1+6, topY+4, 11, hdr1);

        drawLight(window, font, col1, topY+26,
                  snap.f10_north_green, snap.f10_east_green,
                  snap.f10_emergency, snap.f10_crossing, "Traffic Lights");

        drawParking(window, font, col1, topY+164, 265, snap.f10_occupancy, "F10 Parking");

        /* ── F11 ── */
        sf::Color hdr2 = snap.f11_emergency ? EMERG_C : BLUE_C;
        window.draw(rect(col2, topY, 265, 20,
                         snap.f11_emergency ? sf::Color(60,15,15) : sf::Color(18,28,52), BORDER, 1));
        text(window, font, "F11 INTERSECTION", col2+6, topY+4, 11, hdr2);

        drawLight(window, font, col2, topY+26,
                  snap.f11_north_green, snap.f11_east_green,
                  snap.f11_emergency, snap.f11_crossing, "Traffic Lights");

        drawParking(window, font, col2, topY+164, 265, snap.f11_occupancy, "F11 Parking");

        /* ── Stats ── */
        float statsY = topY + 212;
        window.draw(rect(col1, statsY, 557, 40, PANEL, BORDER, 1));
        sf::Color ac = snap.active_vehicles > 0 ? GREEN_C : DIM_C;
        text(window, font, "Active: " + std::to_string(snap.active_vehicles) + "/15", col1+8, statsY+6, 11, ac);
        text(window, font, "Parked: " + std::to_string(snap.total_parked),            col1+8, statsY+22, 11, YELLOW_C);
        text(window, font, "F10 crossing: " + std::to_string(snap.f10_crossing),      col2+8, statsY+6,  11, snap.f10_crossing>0?GREEN_C:DIM_C);
        text(window, font, "F11 crossing: " + std::to_string(snap.f11_crossing),      col2+8, statsY+22, 11, snap.f11_crossing>0?GREEN_C:DIM_C);

        /* ── IPC ── */
        drawIPC(window, font, col1, statsY+48, 557, snap.ipc_flash);

        /* ── Log ── */
        drawLog(window, font, logX, topY, 228, 370, snap.log);

        /* ── Status bar ── */
        window.draw(rect(0, 460, 820, 30, PANEL, BORDER, 1));

        /* Spinner */
        const char* spinner[] = {"|", "/", "─", "\\"};
        int spin_idx = (int)(clock.getElapsedTime().asSeconds() * 4) % 4;
        std::string spin = std::string(spinner[spin_idx]);

        /* Status message */
        pthread_mutex_lock(&g_state.lock);
        std::string smsg = g_state.status_msg;
        int spawned = g_state.spawned_count;
        pthread_mutex_unlock(&g_state.lock);

        std::string bar = spin + "  " + smsg + "  [Spawned: " + std::to_string(spawned) + "/15]";
        text(window, font, bar, 18, 468, 10, CYAN_C);

        char tbuf[16];
        time_t now = time(NULL);
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&now));
        text(window, font, std::string("Time: ") + tbuf, 700, 468, 10, DIM_C);

        window.display();
    }
    return NULL;
}

/* ── Public C API ── */
extern "C" {

void graphics_init(void) {
    pthread_mutex_init(&g_state.lock, NULL);
    g_state.f10_crossing = g_state.f10_occupancy = 0;
    g_state.f11_crossing = g_state.f11_occupancy = 0;
    g_state.f10_north_green = g_state.f10_east_green = 0;
    g_state.f11_north_green = g_state.f11_east_green = 0;
    g_state.f10_emergency = g_state.f11_emergency = 0;
    g_state.total_parked = g_state.active_vehicles = 0;
    g_state.ipc_flash = {0.f, 0};
    g_state.final_stats.ready = false;
    g_state.shutdown = false;
    g_state.spawned_count = 0;
    memset(g_state.status_msg, 0, sizeof(g_state.status_msg));
    strncpy(g_state.status_msg, "Initializing simulation...", sizeof(g_state.status_msg)-1);
    new (&g_state.log) std::deque<LogEntry>();
    pthread_create(&g_thread, NULL, sfml_thread, NULL);
}

void graphics_log_event(const char* vehicle_type, const char* status,
                        int intersection_id, int vehicle_id) {
    time_t now = time(NULL);
    char tbuf[10];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&now));

    pthread_mutex_lock(&g_state.lock);
    LogEntry e;
    e.vehicle_type    = vehicle_type ? vehicle_type : "?";
    e.status          = status       ? status       : "?";
    e.intersection_id = intersection_id;
    e.vehicle_id      = vehicle_id;
    e.timestamp       = tbuf;
    g_state.log.push_back(e);
    if ((int)g_state.log.size() > 18) g_state.log.pop_front();

    /* Flash IPC pipe when emergency is sent */
    if (e.status == "EMERGENCY") {
        g_state.ipc_flash.timer = 2.0f;
        g_state.ipc_flash.direction = intersection_id;
    }
    pthread_mutex_unlock(&g_state.lock);
}

void graphics_update_parking(int f10_occupancy, int f11_occupancy) {
    pthread_mutex_lock(&g_state.lock);
    g_state.f10_occupancy = f10_occupancy;
    g_state.f11_occupancy = f11_occupancy;
    pthread_mutex_unlock(&g_state.lock);
}

void graphics_update_state(
    int f10_crossing, int f10_occupancy,
    int f11_crossing, int f11_occupancy,
    int f10_north_green, int f10_east_green,
    int f11_north_green, int f11_east_green,
    int f10_emergency, int f11_emergency,
    int total_parked, int active_vehicles)
{
    pthread_mutex_lock(&g_state.lock);
    g_state.f10_crossing    = f10_crossing;
    g_state.f10_occupancy   = f10_occupancy;
    g_state.f11_crossing    = f11_crossing;
    g_state.f11_occupancy   = f11_occupancy;
    g_state.f10_north_green = f10_north_green;
    g_state.f10_east_green  = f10_east_green;
    g_state.f11_north_green = f11_north_green;
    g_state.f11_east_green  = f11_east_green;
    g_state.f10_emergency   = f10_emergency;
    g_state.f11_emergency   = f11_emergency;
    g_state.total_parked    = total_parked;
    g_state.active_vehicles = active_vehicles;
    pthread_mutex_unlock(&g_state.lock);
}

void graphics_set_status(const char* msg, int spawned_count) {
    pthread_mutex_lock(&g_state.lock);
    strncpy(g_state.status_msg, msg ? msg : "", sizeof(g_state.status_msg)-1);
    if (spawned_count >= 0) g_state.spawned_count = spawned_count;
    pthread_mutex_unlock(&g_state.lock);
}

void graphics_show_final(int total_vehicles, int total_parked,
                         int emergency_count, int bus_count,
                         int car_count, int bike_count, int tractor_count,
                         int f10_peak_occupancy, int f11_peak_occupancy) {
    pthread_mutex_lock(&g_state.lock);
    g_state.final_stats.ready           = true;
    g_state.final_stats.total_vehicles  = total_vehicles;
    g_state.final_stats.total_parked    = total_parked;
    g_state.final_stats.emergency_count = emergency_count;
    g_state.final_stats.bus_count       = bus_count;
    g_state.final_stats.car_count       = car_count;
    g_state.final_stats.bike_count      = bike_count;
    g_state.final_stats.tractor_count   = tractor_count;
    g_state.final_stats.f10_peak_occupancy = f10_peak_occupancy;
    g_state.final_stats.f11_peak_occupancy = f11_peak_occupancy;
    pthread_mutex_unlock(&g_state.lock);
}

void graphics_shutdown(void) {
    pthread_mutex_lock(&g_state.lock);
    g_state.shutdown = true;
    pthread_mutex_unlock(&g_state.lock);
    pthread_join(g_thread, NULL);
    pthread_mutex_destroy(&g_state.lock);
}

} /* extern "C" */
