#include <pebble.h>

#include "glyphs.h"
#include "fb.h"
#include "util.h"
#include "simple-squared.h"

#if 1
#undef APP_LOG
#define APP_LOG(...)
#define START_TIME_MEASURE() {
#define END_TIME_MEASURE(x) }
#define DBG(...)
#else
static unsigned int get_time(void) {
   time_t s;
   uint16_t ms;
   time_ms(&s, &ms);
   return (s & 0xfffff) * 1000 + ms;
}

#define DBG(...) APP_LOG(APP_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define START_TIME_MEASURE() \
   {                         \
   unsigned tm_0 = get_time()
#define END_TIME_MEASURE(x)                                       \
   unsigned tm_1 = get_time();                                    \
   APP_LOG(APP_LOG_LEVEL_DEBUG, "%s: took %dms", x, tm_1 - tm_0); \
   }
#endif

struct App {
   struct tm t;
   Window *w;
   uint8_t fg;
   uint8_t fg2;
   uint8_t bg;
   uint8_t bg2;
};

struct App *g;

enum Adjust {
   ADJ_LEFT,
   ADJ_CENTER,
   ADJ_RIGHT
};

static int drawString(GPoint p, char *s, int size, uint8_t c) {
   int x = 0;
   int psize = size < 0 ? -size : size;
   for (int i = 0; s[i]; i++)
      x += drawGlyph(GPoint(p.x+x, p.y), char2index(s[i]), size, c) + psize + 1;
   return x - psize - 1; // adjust for extra space at the end
}

static int drawStringAdjusted(enum Adjust a, GPoint p, char *s, int size, uint8_t c) {
   int w = drawString(p, s, -size, c);
   switch (a) {
      case ADJ_LEFT:
         break;
      case ADJ_CENTER:
         p.x = FBW / 2 - w / 2;
         break;
      case ADJ_RIGHT:
         p.x = FBW - p.x - w;
         break;
   }
   return drawString(p, s, size, c);
}

static int drawNumber(GPoint p, unsigned num, int s, uint8_t c) {
   char buf[10];
   snprintf(buf, sizeof(buf), "%u", num);
   return drawString(p, buf, s, c);
}

static int drawNumberAdjusted(enum Adjust a, GPoint p, unsigned num, int s, uint8_t c) {
   int w = drawNumber(p, num, -s, c);
   switch (a) {
      case ADJ_LEFT:
         break;
      case ADJ_CENTER:
         p.x = FBW / 2 - w / 2;
         break;
      case ADJ_RIGHT:
         p.x = FBW - p.x - w;
         break;
   }
   return drawNumber(p, num, s, c);
}

static void draw(void) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "drawing");
   START_TIME_MEASURE();

   uint8_t bg = g->bg;
   uint8_t bg2 = g->bg2;
   uint8_t fg = g->fg;
   uint8_t fg2 = g->fg2;

   fbClear(bg);

   static char *weekday[] = {
      "sunday",
      "monday",
      "tuesday",
      "wednesday",
      "thursday",
      "friday",
      "saturday"
   };

   static char *month[] = {
      "jan",
      "feb",
      "mar",
      "apr",
      "may",
      "jun",
      "jul",
      "aug",
      "sep",
      "oct",
      "nov",
      "dec"
   };

   const int margin = 9;

   GPoint p = GPoint(0, margin);
   drawNumberAdjusted(ADJ_CENTER, p, g->t.tm_hour, 12, fg);

   p = GPoint(10, 168 - margin - 12 * 5);
   drawNumberAdjusted(ADJ_CENTER, p, g->t.tm_min, 12, fg);

   p = GPoint(0, FBH / 2 - 1 * 5); // 1 * 5 is (2/2)*5
   fbFillRect(GRect(0, p.y - 3, FBW, 16), bg2);
   switch (g->t.tm_min & 1) {
      case 0: // day-of-month 3-letter-month
         {
            char buf[14];
            snprintf(buf, sizeof(buf), "%d %s", g->t.tm_mday, month[g->t.tm_mon]);
            drawStringAdjusted(ADJ_CENTER, p, buf, 2, fg2);
            break;
         }
      case 1: // weekday
         drawStringAdjusted(ADJ_CENTER, p, weekday[g->t.tm_wday], 2, fg2);
         break;
   }

   END_TIME_MEASURE("drawing");
}

static void update(Layer *layer, GContext *ctx) {
   GBitmap *bmp = graphics_capture_frame_buffer(ctx);
   fbSet(gbitmap_get_data(bmp));

   APP_LOG(APP_LOG_LEVEL_DEBUG, "locked framebuffer");

   draw();

   graphics_release_frame_buffer(ctx, bmp);
   fbSet(NULL);
}

static void tick(struct tm *tick_time, TimeUnits units_changed) {
   g->t = *tick_time;
   layer_mark_dirty(window_get_root_layer(g->w));
}

static void window_load(Window *w) {
   layer_set_update_proc(window_get_root_layer(w), update);
}

static void window_unload(Window *w) {
}

static void init_time(struct App *a) {
   time_t t = time(NULL);
   struct tm *tm = localtime(&t);
   a->t = *tm;
}

static void set_colors(uint8_t bg, uint8_t fg, uint8_t bg2, uint8_t fg2) {
   g->fg = fg;
   g->bg = bg;
   g->fg2 = fg2;
   g->bg2 = bg2;
   APP_LOG(APP_LOG_LEVEL_DEBUG, "bg=%02x, fg=%02x, bg2=%02x, fg2=%02x", (unsigned)bg, (unsigned)fg, (unsigned)bg2, (unsigned)fg2);
   if (g->w) {
      layer_mark_dirty(window_get_root_layer(g->w));
   }
   persist_write_int(0, ((unsigned)fg2 << 24) | ((int)bg2 << 16) | ((int)fg << 8) | bg);
}

static void set_colors_rgb(int bg, int fg, int bg2, int fg2) {
   set_colors(GColorFromRGB(bg >> 16, (bg & 0xff00) >> 8, bg & 0xff).argb,
              GColorFromRGB(fg >> 16, (fg & 0xff00) >> 8, fg & 0xff).argb,
              GColorFromRGB(bg2>> 16, (bg2& 0xff00) >> 8, bg2& 0xff).argb,
              GColorFromRGB(fg2>> 16, (fg2& 0xff00) >> 8, fg2& 0xff).argb);
}

static void inbox_received_handler(DictionaryIterator *iter, void *data) {
   Tuple *tfg = dict_find(iter, KEY_FG);
   Tuple *tfg2 = dict_find(iter, KEY_FG2);
   Tuple *tbg = dict_find(iter, KEY_BG);
   Tuple *tbg2 = dict_find(iter, KEY_BG2);
   if(tfg && tbg && tfg2 && tbg2) {
      int fg = tfg->value->uint32;
      int bg = tbg->value->uint32;
      int fg2= tfg2->value->uint32;
      int bg2= tbg2->value->uint32;
      set_colors_rgb(bg, fg, bg2, fg2);
   }
}

static void init_config(struct App *a) {
   int colors = persist_read_int(0);
   int bg = colors&0xff;
   int fg = (colors >> 8) & 0xff;
   int bg2 = (colors >> 16) & 0xff;
   int fg2 = (colors >> 24) & 0xff;
   if (fg == bg) {
      bg = 0xf0; // red
      fg = 0xff; // white
   }
   if (fg2 == bg2) {
      bg2 = 0xc0; // black
      fg2 = 0xff; // white
   }
   set_colors(bg, fg, bg2, fg2);
   app_message_register_inbox_received(inbox_received_handler);
   app_message_open(app_message_inbox_size_maximum(),
         app_message_outbox_size_maximum());
}

static void init(struct App *a) {
   g = a;
   a->w = window_create();
   init_time(a);
   tick_timer_service_subscribe(MINUTE_UNIT, tick);
   window_set_user_data(a->w, a);
   window_set_window_handlers(a->w,
                              (WindowHandlers){
                                 .load = window_load, .unload = window_unload,
                              });
   init_config(a);
   window_stack_push(a->w, true);
}

static void fini(struct App *a) {
   tick_timer_service_unsubscribe();
   window_destroy(a->w);
   a->w = NULL;
   g = NULL;
}

int main(void) {
   struct App a;
   memset(&a, 0, sizeof(a));
   init(&a);
   app_event_loop();
   fini(&a);
   return 0;
}
