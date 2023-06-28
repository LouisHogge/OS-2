#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <sys/time.h>

#include "emulator.h"

#define MAX_PATH_LEN 4096

static GtkWidget* main_window = NULL;
static char filename[MAX_PATH_LEN];
static Computer computer;
static bool computer_init = false;
static GtkWidget* code_view;
static GtkListStore* code_store;
static GtkWidget* memory_view;
static GtkListStore* memory_store;
static double temp_frequency;
static double frequency = 1.0;

static GdkPixbuf* pixels_buf = NULL;
static GtkWidget* screen_window = NULL;
static GtkWidget* canvas = NULL;
static int screen_area;
static int screen_width;
static int screen_height;

#define NB_REGS_STORES 3
static int regs_store_index = 0;
static GtkWidget* regs_views[NB_REGS_STORES];
static GtkListStore* regs_stores[NB_REGS_STORES];
static int regs_stores_starts[NB_REGS_STORES];
static int regs_stores_ends[NB_REGS_STORES];

static GtkWidget* address_search;
static GtkWidget* address_button;
static int selected_address = 0x0;

static bool running = false;
static bool open_blocked = false;
static bool run_blocked = false;
static bool run_paused = false;
static bool stop_emulator = false;
static bool first_open = true;
static bool frequency_window_opened = false;

pthread_mutex_t computer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t paused_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t frequency_mutex = PTHREAD_MUTEX_INITIALIZER;

enum

{
  MEMORY_TABLE_COL_ADDRESS = 0,
  MEMORY_TABLE_COL_VAL,
  MEMORY_TABLE_NUM_COLS
};


static GtkTreeModel* create_memory_model (void){

    GtkListStore *store = gtk_list_store_new (MEMORY_TABLE_NUM_COLS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);
    memory_store = store;
  
    for(int i = 0; i < 8; i++){
      
      GtkTreeIter iter;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          MEMORY_TABLE_COL_ADDRESS, "",
                          MEMORY_TABLE_COL_VAL, "",
                          -1);
    }

    return GTK_TREE_MODEL (store);
}

static GtkWidget* create_memory_view_and_model (void){

  GtkWidget *view = gtk_tree_view_new ();
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Address",  
                                               renderer,
                                               "text", MEMORY_TABLE_COL_ADDRESS,
                                               NULL);

  
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Value",  
                                               renderer,
                                               "text", MEMORY_TABLE_COL_VAL,
                                               NULL);

  GtkTreeModel *model = create_memory_model ();

  gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
  g_object_unref(model);

  return view;
}

enum
{
  CODE_TABLE_COL_ADDRESS = 0,
  CODE_TABLE_COL_PC,
  CODE_TABLE_COL_VAL,
  CODE_TABLE_COL_DISASSEMBLY,
  CODE_TABLE_NUM_COLS
};


static GtkTreeModel* create_code_model (void){

    GtkListStore *store = gtk_list_store_new (CODE_TABLE_NUM_COLS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);
    code_store = store;
  
    for(int i = 0; i < 8; i++){
      
      GtkTreeIter iter;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          CODE_TABLE_COL_ADDRESS, "",
                          CODE_TABLE_COL_PC, "",
                          CODE_TABLE_COL_VAL, "",
                          CODE_TABLE_COL_DISASSEMBLY, "",
                          -1);
    }

    return GTK_TREE_MODEL (store);
}



static GtkWidget* create_code_view_and_model (void){

  GtkWidget *view = gtk_tree_view_new ();
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Address",  
                                               renderer,
                                               "text", CODE_TABLE_COL_ADDRESS,
                                               NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "PC?",  
                                               renderer,
                                               "text", CODE_TABLE_COL_PC,
                                               NULL);
  
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Value",  
                                               renderer,
                                               "text", CODE_TABLE_COL_VAL,
                                               NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Disassemby",  
                                               renderer,
                                               "text", CODE_TABLE_COL_DISASSEMBLY,
                                               NULL);

  GtkTreeModel *model = create_code_model ();

  gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
  g_object_unref(model);

  return view;
}

enum{
    REGS_TABLE_COL_REG = 0,
    REGS_TABLE_COL_VAL,
    REGS_TABLE_NUM_COLS
};

static GtkTreeModel* create_regs_model (int start, int end){

    GtkListStore *store = gtk_list_store_new (REGS_TABLE_NUM_COLS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);
    regs_stores[regs_store_index++] = store;
  
    for(int i = start; i <= end; i++){
      
      GtkTreeIter iter;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          REGS_TABLE_COL_REG, reg_symbols[i],
                          REGS_TABLE_COL_VAL, "00000000",
                          -1);
    }

    return GTK_TREE_MODEL (store);
}

static GtkWidget* create_regs_view_and_model (int start, int end){

  GtkWidget *view = gtk_tree_view_new ();
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Register",  
                                               renderer,
                                               "text", REGS_TABLE_COL_REG,
                                               NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Value",  
                                               renderer,
                                               "text", REGS_TABLE_COL_VAL,
                                               NULL);



  GtkTreeModel *model = create_regs_model (start, end);

  gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
  g_object_unref(model);

  return view;
}

static gboolean event_key_pressed (GtkWidget* widget,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      GtkEventControllerKey* event_controller){
    
    if(keyval >= 128 || !computer_init)
        return TRUE;
    
    pthread_mutex_lock(&computer_mutex);
    raise_interrupt(&computer, 0, keyval);
    pthread_mutex_unlock(&computer_mutex);
    fprintf(stderr, "key pressed event %c %d %c %d\n", keyval, keyval, keycode, keycode);
    return TRUE;
}

static gboolean event_key_released (GtkWidget* widget,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      GtkEventControllerKey* event_controller){
    
    if(keyval >= 128 || !computer_init)
        return FALSE;
    
    pthread_mutex_lock(&computer_mutex);
    raise_interrupt(&computer, 1, keyval);
    pthread_mutex_unlock(&computer_mutex);
    fprintf(stderr, "key released event %c\n", keyval);
    return FALSE;
}

void update_memory_state(){

    gtk_list_store_clear(memory_store);
    int start = selected_address;
    
    for(int addr = start; addr <= start + 28 ; addr += 4){
      
      pthread_mutex_lock(&computer_mutex);
      int word = get_word(&computer, addr);
      pthread_mutex_unlock(&computer_mutex);
      
      char buf[10];
      char buf2[10];
      char disassembly[512];
      
      sprintf(buf, "%.8x", addr);
      sprintf(buf2, "%.8x", word);
      
      GtkTreeIter iter;
      gtk_list_store_append (memory_store, &iter);
      gtk_list_store_set (memory_store, &iter,
                          MEMORY_TABLE_COL_ADDRESS, buf,
                          MEMORY_TABLE_COL_VAL, buf2,
                          -1);
    }
}


void update_code_state(){

    gtk_list_store_clear(code_store);
    int pc = computer.cpu.program_counter;
    int start = pc - 4;
    
    if(computer.cpu.program_counter == 0) 
    	start = pc;
    
    for(int addr = start; addr <= start + 28 ; addr += 4){
      
      pthread_mutex_lock(&computer_mutex);
      int instruction = get_word(&computer, addr);
      pthread_mutex_unlock(&computer_mutex);
      
      char buf[10];
      char buf2[10];
      char disassembly[512];
      
      sprintf(buf, "%.8x", addr);
      sprintf(buf2, "%.8x", instruction);
      
      disassemble(instruction, disassembly);
      
      GtkTreeIter iter;
      gtk_list_store_append (code_store, &iter);
      gtk_list_store_set (code_store, &iter,
                          CODE_TABLE_COL_ADDRESS, buf,
                          CODE_TABLE_COL_PC, (addr == pc) ? "X": "",
                          CODE_TABLE_COL_VAL, buf2,
                          CODE_TABLE_COL_DISASSEMBLY, disassembly,
                          -1);
    }
}

void update_regs_state(){
    
    char buf[10];
    
    for(int i = 0; i < 3; i++){
        
        gtk_list_store_clear(regs_stores[i]);
        
        for(int j = regs_stores_starts[i]; j <= regs_stores_ends[i]; j++){
            
            pthread_mutex_lock(&computer_mutex);
            sprintf(buf, "%.8x", get_register(&computer, j));
            pthread_mutex_unlock(&computer_mutex);
            
            GtkTreeIter iter;
            gtk_list_store_append (regs_stores[i], &iter);
            gtk_list_store_set (regs_stores[i], &iter,
                               REGS_TABLE_COL_REG, reg_symbols[j],
                               REGS_TABLE_COL_VAL, buf,
                               -1);
        } 
    
    }
}

void init_screen(){

    int n_channels = gdk_pixbuf_get_n_channels (pixels_buf);
    int rowstride = gdk_pixbuf_get_rowstride (pixels_buf);
    guchar* pixels = gdk_pixbuf_get_pixels (pixels_buf);
    
    int row_byte_length = screen_width * 4;
    
    for(int y = 0; y < screen_height; y++){
        for(int x = 0; x < screen_width; x++){
        
            pthread_mutex_lock(&computer_mutex);
            unsigned int pixel = get_word(&computer, 
                                          computer.program_memory_size
                                          + y * row_byte_length
                                          + x * 4);
            pthread_mutex_unlock(&computer_mutex);
            
            guchar* p = pixels + y * rowstride + x * n_channels;
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
    } 
    
    gtk_picture_set_pixbuf((GtkPicture*) canvas, pixels_buf);
}

void update_screen(){

    if(first_open)
        return;
    
    Computer* c = &computer;
    pthread_mutex_lock(&computer_mutex);
    
    if(c -> latest_accessed < c -> program_memory_size
        || c -> latest_accessed >= c -> program_memory_size 
                                   + c -> video_memory_size){
        
        pthread_mutex_unlock(&computer_mutex);
        return;
    }
  
    
    unsigned int video_memory_offset = c-> program_memory_size;
    unsigned int pixel_start = c -> latest_accessed 
                               - (((c -> latest_accessed) 
                                    - video_memory_offset)) % 4;
    
    unsigned int pixel = get_word(&computer, 
                                  pixel_start);
    
    pthread_mutex_unlock(&computer_mutex);
                                  
    unsigned int x = ((pixel_start - video_memory_offset) / 4)  % screen_width;
    unsigned int y = ((pixel_start - video_memory_offset) / 4)  / screen_width;
   
    int n_channels = gdk_pixbuf_get_n_channels (pixels_buf);
    int rowstride = gdk_pixbuf_get_rowstride (pixels_buf);
    guchar *pixels = gdk_pixbuf_get_pixels (pixels_buf);
            
    guchar *p = pixels + y * rowstride + x * n_channels;
    p[0] = pixel & 0xff;
    p[1] = (pixel >> 8) & 0xff;
    p[2] = (pixel >> 16) & 0xff;
     
    gtk_picture_set_pixbuf((GtkPicture*) canvas, pixels_buf);
}


gboolean update_display_state(gpointer* par){
    
    bool do_screen = (bool) (void*) par;
    
    if(!computer_init)
        return FALSE;
        
    update_code_state();
    update_memory_state();
    update_regs_state();
    
    if(do_screen)
        update_screen();
    
    return FALSE;
}

gboolean full_update_display_state(){
    
    if(!computer_init)
        return FALSE;
        
    update_code_state();
    update_memory_state();
    update_regs_state();
    init_screen();
    
    return FALSE;
}



void update_memory_address(GtkWidget *widget, gpointer data){

    GtkEntryBuffer* buffer = gtk_entry_get_buffer((GtkEntry*) address_search);
    const char* text = gtk_entry_buffer_get_text(buffer);
    
    char buf[9];
    memset(buf, 0, 9);
    strncpy(buf, text, 8);
    long addr = strtol(buf, NULL, 16);

    if(addr > 0)
        selected_address = addr;
        	
    update_display_state((gpointer) (void*) TRUE);
}

void make_responsive(GtkWidget* window){

    GtkEventController* event_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(window, event_controller);
    
    g_signal_connect_object (event_controller, "key-pressed",
                           G_CALLBACK (event_key_pressed),
                           NULL, G_CONNECT_SWAPPED);
    
    g_signal_connect_object (event_controller, "key-released",
                           G_CALLBACK (event_key_released),
                           NULL, G_CONNECT_SWAPPED);

}

void* open_thread(void *arg) {

    char* filename = (char*) arg;
    FILE* fp = fopen(filename, "rb");
            
    if(fp == NULL)
        return NULL;
        
    while(running && !run_paused) ;
    stop_emulator = false;
        
    if(computer_init)
        free_computer(&computer);
    
    init_computer(&computer, PROGRAM_MEMORY_SZ, VIDEO_MEMORY_SZ, KERNEL_MEMORY_SZ);
    load(&computer, fp);
    fclose(fp);
    fp = fopen("interrupt_handler.asm.bin", "rb");
    load_interrupt_handler(&computer, fp);
    computer_init = true;
    
    init_screen();
    g_idle_add((GSourceFunc) update_display_state, (gpointer) (void*) FALSE);
    
    open_blocked = false;
    run_blocked = false;
    
    pthread_mutex_trylock(&paused_mutex);
    pthread_mutex_unlock(&paused_mutex);
    
    pthread_exit(NULL);
}

static void on_open_response (GtkDialog *dialog, int response){
        
    if (response == GTK_RESPONSE_ACCEPT){
    
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        g_autoptr(GFile) file = gtk_file_chooser_get_file (chooser);
        char* name = g_file_get_path(file);
        strncpy(filename, name, MAX_PATH_LEN);
        g_free(name);
        
        pthread_t thread;
        open_blocked = true;
        run_blocked = true;
        
        if(!first_open){
            stop_emulator = true;
        }
        
        first_open = false;
        pthread_create(&thread, NULL, open_thread, (void*) filename);
    }
    
    if(response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_CANCEL)
        gtk_window_destroy (GTK_WINDOW (dialog));
}


static void open_file_selector(GtkWidget *widget, gpointer data){
    
    if(open_blocked)
        return;

    GtkWidget* dialog;
    dialog = gtk_file_chooser_dialog_new ("Open File",
                                        GTK_WINDOW(main_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        "_Open",
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
    
    g_signal_connect (dialog, "response", G_CALLBACK (on_open_response), NULL);
    gtk_widget_show(dialog);
}

static inline unsigned long get_time_millis(){

    struct timeval  tv;
    gettimeofday(&tv, NULL);
    double time_in_mill =  (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
    
    return time_in_mill;
}

void* execute_thread(void* arg){
    
    run_blocked = true;
    int pc = computer.cpu.program_counter;
    bool halted = false;
    int program_size = computer.program_size;
    char buf[1024];
    running = true;
    unsigned long prev_time = get_time_millis();
    unsigned long now_time;
    double f;
    
    while(!halted && (pc < program_size) 
                     || ((pc > computer.program_memory_size
                          + computer.video_memory_size)
                        && (pc < computer.memory_size))){
        
    	if(run_paused){
    	    
    	    run_blocked = false;
    	    pthread_mutex_lock(&paused_mutex);
    	    run_blocked = true;
    	    pthread_mutex_unlock(&paused_mutex);
    	    run_paused = false;
    	}
    	
    	if(stop_emulator) {
    	    
    	    stop_emulator = false;   
    	    break;
    	}
        
        pthread_mutex_lock(&computer_mutex);
        int instruction = get_word(&computer, pc);
        pthread_mutex_unlock(&computer_mutex);
        
        pthread_mutex_lock(&computer_mutex);
        execute_step(&computer);
        halted = computer.halted;
        pc = computer.cpu.program_counter;
        pthread_mutex_unlock(&computer_mutex);
        
        pthread_mutex_lock(&frequency_mutex);
        f = frequency;
        pthread_mutex_unlock(&frequency_mutex);
        
        if(f < 0 || f > 10){
            
            now_time = get_time_millis();

            if(now_time - prev_time > 100){
            
                prev_time = now_time;
                g_idle_add((GSourceFunc) full_update_display_state, NULL);
            }
        }
        
        else
            g_idle_add((GSourceFunc) update_display_state, (gpointer) (void*) TRUE);
       

        if(f > 0){
            
            int to_wait = 1000000 / f;
            
            if(to_wait > 800000){
                
                int us;
                
                do{
                    
                    us = (to_wait > 100000) ? 100000 : to_wait; 
                    usleep(us);
                    to_wait -= us;
                    
                    if(run_paused)
                        continue;
                    
                    if(stop_emulator)
                        break;
                    
                } while(to_wait > 0);
            
            }
            
            else
            	usleep(to_wait);
        }
    }
    
    if(f < 0 || f > 10)
        g_idle_add((GSourceFunc) full_update_display_state, NULL);
      
    run_blocked = false;
    running = false;
    pthread_exit(NULL);
}

void start_executing(GtkWidget *widget, gpointer data){

    if(run_blocked || !computer_init)
        return;
        
    if(run_paused){
    
        pthread_mutex_unlock(&paused_mutex);
        run_paused = false;
        return;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, execute_thread, NULL);
}

void pause_execution(GtkWidget *widget, gpointer data){
    
    static bool pausing = false;
    
    if(pausing || run_paused || !running)
        return;
    
    pausing = true;
    pthread_mutex_trylock(&paused_mutex);
    run_paused = true;
    pausing = false;  
}

void reset_emulator(GtkWidget *widget, gpointer data){
    
    if(first_open)
        return;
        
    open_blocked = true;
    run_blocked = true;
    stop_emulator = true;
    pthread_mutex_trylock(&paused_mutex);
    pthread_mutex_unlock(&paused_mutex);
    run_paused = false;
    
    pthread_t thread;
    pthread_create(&thread, NULL, open_thread, (void*) filename);
}

void single_step(GtkWidget *widget, gpointer data){

    if(!computer_init)
        return;
    
    int pc = computer.cpu.program_counter;
    bool halted = computer.halted;
        
    if(!halted && (pc < computer.program_size) 
                     || ((pc > computer.program_memory_size
                          + computer.video_memory_size)
                        && (pc < computer.memory_size))){

        pause_execution(NULL, NULL);
        execute_step(&computer);
        g_idle_add((GSourceFunc) update_display_state, (gpointer) (void*) TRUE);
    }
}

void close_frequency(GtkWidget *widget, gpointer data){
    
    frequency_window_opened = false;
}

#define NB_RADIOS 6

static double const freq_values[NB_RADIOS] = {0.1, 1.0, 10.0, 1000.0, 1000000.0, -1.0};

void set_temp_frequency(GtkWidget *widget, gpointer data){
    
    if(!gtk_toggle_button_get_active((GtkToggleButton*) widget))
        return;
    
    temp_frequency = *((double*) data);
}

void set_frequency(GtkWidget *widget, gpointer data){
    
    pthread_mutex_lock(&frequency_mutex);
    frequency = temp_frequency;
    full_update_display_state();
    pthread_mutex_unlock(&frequency_mutex);
    
    frequency_window_opened = false;
    gtk_window_destroy (GTK_WINDOW (data));
}


void open_frequency_window(GtkWidget *widget, gpointer data){
   
    GtkWidget *window, *ok_button;
    GtkWidget* frequency_radio[NB_RADIOS];
    
    if(frequency_window_opened)
        return;
          
    frequency_window_opened = true;
    temp_frequency = frequency;
    
    window = gtk_window_new();
    gtk_window_set_title (GTK_WINDOW (window), "Choose a CPU frequency");
    gtk_window_set_default_size (GTK_WINDOW (window), 100, 100);
    
    make_responsive(window);
    GtkWidget* hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
    ok_button = gtk_button_new_with_label("OK");
    
    gtk_window_set_child (GTK_WINDOW (window), vbox);
    gtk_box_append(GTK_BOX(vbox), hbox);
    g_signal_connect(hbox, "destroy", G_CALLBACK (close_frequency), NULL);
    
    
    frequency_radio[0]=  gtk_toggle_button_new_with_label("1/10 Hz");
    frequency_radio[1] = gtk_toggle_button_new_with_label("1 Hz");
    frequency_radio[2] = gtk_toggle_button_new_with_label("10 Hz");
    frequency_radio[3] = gtk_toggle_button_new_with_label("1 kHz");
    frequency_radio[4] = gtk_toggle_button_new_with_label("1 MHz");
    frequency_radio[5] = gtk_toggle_button_new_with_label("Unbounded");
    
    for(int i = 0; i < NB_RADIOS; i++){
               
        g_signal_connect(frequency_radio[i], "toggled", G_CALLBACK (set_temp_frequency), 
                                              (gpointer) ((void*) (freq_values + i)));
        
        gtk_box_append(GTK_BOX(hbox), frequency_radio[i]);
    }
    
    for(int i = 0; i < NB_RADIOS; i++)
        for(int j = i + 1; j < NB_RADIOS; j++)
            if(i!= j)
                gtk_toggle_button_set_group((GtkToggleButton*) frequency_radio[i], 
                                            (GtkToggleButton*) frequency_radio[j]);
    
    gtk_box_append(GTK_BOX(vbox), ok_button);
    g_signal_connect(ok_button, "clicked", G_CALLBACK (set_frequency), 
                                           (gpointer) window);
    gtk_widget_show(window);
}

#undef NB_RADIOS


void open_drawing_window(GtkWidget *widget, gpointer data){

    GtkWidget *window;
    
    screen_area = VIDEO_MEMORY_SZ / 4;
    screen_height = sqrt((screen_area * 2.0) / 3.0);
    screen_width = screen_area / screen_height;
    
    window = gtk_window_new();
    screen_window = window;
    gtk_window_set_title (GTK_WINDOW (window), "Screen");
    gtk_window_set_deletable(GTK_WINDOW (window), FALSE);
    pixels_buf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 
                                FALSE, 8, 
                                screen_width, screen_height);
    
    
    canvas = gtk_picture_new_for_pixbuf(pixels_buf);
    update_screen();
    //gtk_picture_set_pixbuf((GtkPicture*) canvas, pixels_buf);
    gtk_window_set_child (GTK_WINDOW (window), canvas);
    gtk_widget_set_size_request(canvas, screen_width, screen_height);
    make_responsive(window);
    gtk_widget_show(window);
}

static void activate(GtkApplication *app, gpointer user_data){

    GtkWidget *window, *file_button, *box1, *box2, *grid, *hbox;
    GtkWidget *hbox2, *action_box, *action_bar, *run_button;
    GtkWidget *vbox, *pause_button, *regs_table, *step_button;
    GtkWidget *reset_button, *frequency_button;

    window = gtk_application_window_new (app);
    main_window = window;
    gtk_window_set_title (GTK_WINDOW (window), "Beta-emulator");
    //gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
    
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign (vbox, GTK_ALIGN_BASELINE);
    gtk_widget_set_valign (vbox, GTK_ALIGN_BASELINE);
    
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
    
    box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign (box1, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box1, GTK_ALIGN_CENTER);
    
    box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box2, GTK_ALIGN_CENTER);
    
    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (hbox2, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (hbox2, GTK_ALIGN_CENTER);
    
    action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (action_box, GTK_ALIGN_BASELINE);
    gtk_widget_set_valign (action_box, GTK_ALIGN_BASELINE);

    file_button = gtk_button_new_with_label ("Choose\n     file");
    address_button = gtk_button_new_with_label ("OK");
    address_search = gtk_entry_new();
    action_bar = gtk_action_bar_new();
    run_button = gtk_button_new_with_label("Run");
    step_button = gtk_button_new_with_label ("Single\n  step");
    pause_button = gtk_button_new_with_label("Pause");
    reset_button = gtk_button_new_with_label("Reset");
    frequency_button = gtk_button_new_with_label ("       Set\nfrequency");
    
    gtk_window_set_child (GTK_WINDOW (window),vbox);
    
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, file_button);
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, reset_button);
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, run_button);
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, step_button);
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, pause_button);
    gtk_action_bar_pack_start((GtkActionBar*) action_bar, frequency_button);

    g_signal_connect (run_button, "clicked", G_CALLBACK (start_executing), NULL);
    g_signal_connect (step_button, "clicked", G_CALLBACK (single_step), NULL);
    g_signal_connect (file_button, "clicked", G_CALLBACK (open_file_selector), NULL);
    g_signal_connect (pause_button, "clicked", G_CALLBACK (pause_execution), NULL);
    g_signal_connect (reset_button, "clicked", G_CALLBACK (reset_emulator), NULL);
    g_signal_connect (frequency_button, "clicked", G_CALLBACK (open_frequency_window), NULL);
    g_signal_connect (address_button, "clicked", G_CALLBACK (update_memory_address), NULL);
    
    code_view = create_code_view_and_model ();
    gtk_tree_view_set_enable_search((GtkTreeView*) code_view, FALSE);
    
    memory_view = create_memory_view_and_model();
    gtk_tree_view_set_enable_search((GtkTreeView*) memory_view, FALSE);
    
    
    regs_views[0] = create_regs_view_and_model (0, 10);
    regs_stores_starts[0] = 0;
    regs_stores_ends[0] = 10;
    gtk_tree_view_set_enable_search((GtkTreeView*) regs_views[0], FALSE);
    
    regs_views[1] = create_regs_view_and_model (11, 21);
    regs_stores_starts[1] = 11;
    regs_stores_ends[1] = 21;
    gtk_tree_view_set_enable_search((GtkTreeView*) regs_views[1], FALSE);
    
    regs_views[2] = create_regs_view_and_model (22, 31);
    regs_stores_starts[2] = 22;
    regs_stores_ends[2] = 31;
    gtk_tree_view_set_enable_search((GtkTreeView*) regs_views[2], FALSE);
    
    gtk_box_append(GTK_BOX (vbox), action_box);
    gtk_box_append(GTK_BOX (vbox), hbox);
    gtk_box_append(GTK_BOX (action_box), action_bar);
    gtk_box_append (GTK_BOX (hbox2), address_search);
    gtk_box_append (GTK_BOX (hbox2), address_button);
    gtk_box_append (GTK_BOX (box2), (GtkWidget*) hbox2);
    gtk_box_append (GTK_BOX (box2), memory_view);
    
    gtk_box_append(GTK_BOX (hbox), code_view);
    gtk_box_append(GTK_BOX (hbox), box2);
    
    for(int i = 0; i < NB_REGS_STORES; i++)
        gtk_box_append(GTK_BOX (hbox), regs_views[i]);
        
    gtk_box_append (GTK_BOX (hbox), box1);
    
    make_responsive(window);
    gtk_widget_show (window);
    
    open_drawing_window(NULL, NULL);
}

int main(int argc, char **argv){

    GtkApplication *app;
    int status;

    app = gtk_application_new ("be.uliege.emulator", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);
    
    if(computer_init)
        free_computer(&computer);
        
  return status;
}
