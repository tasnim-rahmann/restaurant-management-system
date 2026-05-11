/*
* Food Ordering Management System
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Constants ─────────────────────────────────────────────── */
#define MAX_MENU_ITEMS  100
#define MAX_ORDERS      200
#define MAX_CARDS       200
#define ADMIN_PASS      "1234"
#define NAME_LEN        50
#define PASS_LEN        32

/* ─── Data structures ────────────────────────────────────────── */
typedef struct {
    int   serial;
    char  name[NAME_LEN];
    int   quantity;
    float price;
} FoodItem;

typedef struct {
    int   food_serial;
    int   quantity;
} Order;

typedef struct {
    int   card_number;
    float amount;
} CardPayment;

typedef struct {
    FoodItem    menu[MAX_MENU_ITEMS];
    int         menu_count;
    Order       orders[MAX_ORDERS];
    int         order_count;
    CardPayment card_payments[MAX_CARDS];
    int         card_count;
    float       total_cash;
} AppState;

/* ─── Menu array helpers ─────────────────────────────────────── */
static int menu_add(AppState *app, int serial, const char *name, int qty, float price) {
    if (app->menu_count >= MAX_MENU_ITEMS) { puts("  Menu is full."); return 0; }
    FoodItem *f = &app->menu[app->menu_count++];
    f->serial   = serial;
    f->quantity = qty;
    f->price    = price;
    strncpy(f->name, name, NAME_LEN - 1);
    f->name[NAME_LEN - 1] = '\0';
    return 1;
}

/* Returns index of item with matching serial, or -1. */
static int menu_find_index(AppState *app, int serial) {
    for (int i = 0; i < app->menu_count; i++)
        if (app->menu[i].serial == serial) return i;
    return -1;
}

static FoodItem *menu_find(AppState *app, int serial) {
    int idx = menu_find_index(app, serial);
    return idx >= 0 ? &app->menu[idx] : NULL;
}

static int serial_exists(AppState *app, int serial) {
    return menu_find_index(app, serial) >= 0;
}

/*
* Delete by shifting elements left — keeps the array compact.
* Returns 1 on success, 0 if not found.
*/
static int menu_delete(AppState *app, int serial) {
    int idx = menu_find_index(app, serial);
    if (idx < 0) return 0;
    for (int i = idx; i < app->menu_count - 1; i++)
        app->menu[i] = app->menu[i + 1];
    app->menu_count--;
    return 1;
}

/* ─── Input helpers ─────────────────────────────────────────── */
static int read_line(char *buf, int max) {
    if (!fgets(buf, max, stdin)) return -1;
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

static int read_int(const char *prompt) {
    char buf[64];
    int  val;
    while (1) {
        printf("%s", prompt);
        if (read_line(buf, sizeof buf) < 0) return -1;
        if (sscanf(buf, "%d", &val) == 1) return val;
        puts("  Please enter a valid number.");
    }
}

static float read_float(const char *prompt) {
    char  buf[64];
    float val;
    while (1) {
        printf("%s", prompt);
        if (read_line(buf, sizeof buf) < 0) return -1.0f;
        if (sscanf(buf, "%f", &val) == 1 && val > 0) return val;
        puts("  Please enter a valid positive number.");
    }
}

static void read_password(char *buf, int max) {
    printf("  Password: ");
    if (read_line(buf, max) < 0) buf[0] = '\0';
}

/* ─── Display helpers ────────────────────────────────────────── */
static void print_separator(void) {
    puts("  +--------+--------------------------+-----------+----------+");
}

static void print_food_list(AppState *app) {
    puts("\n  Food Menu");
    print_separator();
    printf("  | %-6s | %-24s | %-9s | %-8s |\n", "No.", "Name", "Price", "In Stock");
    print_separator();
    for (int i = 0; i < app->menu_count; i++) {
        printf("  | %-6d | %-24s | %9.2f | %-8d |\n",
               app->menu[i].serial,
               app->menu[i].name,
               app->menu[i].price,
               app->menu[i].quantity);
    }
    print_separator();
    putchar('\n');
}

static void print_order_list(AppState *app) {
    if (app->order_count == 0) { puts("  No orders yet."); return; }
    puts("\n  Current Orders");
    puts("  +----------+--------------------------+-----------+");
    printf("  | %-8s | %-24s | %-9s |\n", "Order No.", "Name", "Qty");
    puts("  +----------+--------------------------+-----------+");
    for (int i = 0; i < app->order_count; i++) {
        FoodItem *item = menu_find(app, app->orders[i].food_serial);
        const char *name = item ? item->name : "(deleted)";
        printf("  | %-8d | %-24s | %-9d |\n",
               i + 1, name, app->orders[i].quantity);
    }
    puts("  +----------+--------------------------+-----------+\n");
}

/* ─── Food-ordering flow ─────────────────────────────────────── */
static void place_order(AppState *app) {
    while (1) {
        print_food_list(app);
        int serial = read_int("  Enter food number (0 to go back): ");
        if (serial == 0) return;

        FoodItem *item = menu_find(app, serial);
        if (!item)               { puts("  Item not found. Please choose from the list."); continue; }
        if (item->quantity == 0) { puts("  Sorry, this item is out of stock."); continue; }

        printf("  %s - $%.2f each (stock: %d)\n", item->name, item->price, item->quantity);
        int qty = read_int("  Enter quantity (0 to cancel): ");
        if (qty == 0)              continue;
        if (qty < 0)             { puts("  Quantity cannot be negative."); continue; }
        if (qty > item->quantity){ puts("  Not enough stock."); continue; }

        float subtotal = item->price * qty;
        printf("  Subtotal: $%.2f\n", subtotal);
        puts("  1. Confirm   2. Cancel");
        if (read_int("  Choice: ") != 1) continue;

        puts("  Payment: 1. Cash   2. Credit Card");
        int payment = read_int("  Choice: ");
        if (payment != 1 && payment != 2) { puts("  Invalid payment option."); continue; }

        if (payment == 2) {
            if (app->card_count >= MAX_CARDS) { puts("  Card storage full."); continue; }
            int card_no = read_int("  Enter card number: ");
            char pin[PASS_LEN];
            printf("  Enter card PIN: ");
            read_line(pin, sizeof pin);
            app->card_payments[app->card_count].card_number = card_no;
            app->card_payments[app->card_count].amount      = subtotal;
            app->card_count++;
            puts("  Card payment recorded.");
        }

        /* Commit order */
        item->quantity -= qty;
        app->total_cash += subtotal;
        if (app->order_count < MAX_ORDERS) {
            app->orders[app->order_count].food_serial = serial;
            app->orders[app->order_count].quantity    = qty;
            app->order_count++;
        }
        printf("\n  Order placed successfully! Total charged: $%.2f\n\n", subtotal);

        puts("  1. Order another item   2. Back to main menu");
        if (read_int("  Choice: ") != 1) return;
    }
}

/* ─── Admin panel ────────────────────────────────────────────── */
static void admin_add_food(AppState *app) {
    char name[NAME_LEN];
    printf("  Food name: ");
    read_line(name, sizeof name);
    if (name[0] == '\0') { puts("  Name cannot be empty."); return; }

    int qty = read_int("  Quantity: ");
    if (qty < 0) { puts("  Quantity cannot be negative."); return; }

    int serial;
    while (1) {
        serial = read_int("  Serial number: ");
        if (!serial_exists(app, serial)) break;
        puts("  Serial already in use. Try another.");
    }

    float price = read_float("  Price: ");
    if (menu_add(app, serial, name, qty, price))
        printf("  '%s' added successfully.\n", name);
}

static void admin_delete_food(AppState *app) {
    print_food_list(app);
    int serial = read_int("  Enter serial number to delete (0 to cancel): ");
    if (serial == 0) return;

    if (menu_delete(app, serial))
        printf("  Item %d deleted.\n", serial);
    else
        puts("  Item not found.");
}

static void admin_backup(AppState *app) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[64];
    strftime(filename, sizeof filename, "backup_%Y-%m-%d.txt", t);

    FILE *f = fopen(filename, "w");
    if (!f) { perror("  Cannot create backup file"); return; }

    char datebuf[64];
    strftime(datebuf, sizeof datebuf, "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "=== Food Ordering System Backup ===\nDate: %s\n\n", datebuf);
    fprintf(f, "Total cash today: $%.2f\n\n", app->total_cash);

    if (app->card_count > 0) {
        fprintf(f, "Card Payments:\n");
        for (int i = 0; i < app->card_count; i++)
            fprintf(f, "  Card %d - $%.2f\n",
                    app->card_payments[i].card_number, app->card_payments[i].amount);
        fputc('\n', f);
    }

    fprintf(f, "Food Inventory:\n");
    for (int i = 0; i < app->menu_count; i++)
        fprintf(f, "  [%d] %-24s qty:%-4d  $%.2f\n",
                app->menu[i].serial, app->menu[i].name,
                app->menu[i].quantity, app->menu[i].price);

    fclose(f);
    printf("  Backup saved to '%s'\n", filename);
}

static void admin_panel(AppState *app) {
    char pass[PASS_LEN];
    read_password(pass, sizeof pass);
    if (strcmp(pass, ADMIN_PASS) != 0) { puts("  Incorrect password."); return; }

    int running = 1;
    while (running) {
        puts("\n  === Admin Panel ===");
        puts("  1. Total cash today");
        puts("  2. View card payments");
        puts("  3. Add food item");
        puts("  4. Delete food item");
        puts("  5. View food list");
        puts("  6. Count items");
        puts("  7. Backup data");
        puts("  8. View current orders");
        puts("  0. Back to main menu");

        switch (read_int("\n  Choice: ")) {
            case 1:
                printf("\n  Today's total cash: $%.2f\n", app->total_cash);
                break;
            case 2:
                if (app->card_count == 0) { puts("  No card payments recorded."); break; }
                puts("\n  Card Payments:");
                for (int i = 0; i < app->card_count; i++)
                    printf("    Card %-10d  $%.2f\n",
                           app->card_payments[i].card_number, app->card_payments[i].amount);
                break;
            case 3: admin_add_food(app);   break;
            case 4: admin_delete_food(app); break;
            case 5: print_food_list(app);   break;
            case 6: printf("\n  Total items: %d\n", app->menu_count); break;
            case 7: admin_backup(app);      break;
            case 8: print_order_list(app);  break;
            case 0: running = 0; break;
            default: puts("  Please choose a valid option.");
        }
    }
}

/* ─── Main ───────────────────────────────────────────────────── */
int main(void) {
    AppState app = {0};

    /* Seed initial menu */
    menu_add(&app,  5, "Burger",       23,  175.00f);
    menu_add(&app,  6, "Pizza",        13,  250.00f);
    menu_add(&app,  1, "Hot Cake",      8,  475.00f);
    menu_add(&app,  2, "Coffee",       46,  325.00f);
    menu_add(&app,  3, "Ice Cream",    46,  230.00f);
    menu_add(&app,  4, "Sandwich",     34,  320.00f);
    menu_add(&app,  7, "Grill",         7,  100.00f);
    menu_add(&app,  8, "Pancakes",    121,  350.00f);
    menu_add(&app,  9, "Cold Drinks",  73,  120.00f);

    puts("=========================================");
    puts("  FOOD ORDERING MANAGEMENT SYSTEM");
    puts("=========================================\n");

    int running = 1;
    while (running) {
        puts("  === Main Menu ===");
        puts("  1. Place an order");
        puts("  2. Admin panel");
        puts("  3. Exit");

        switch (read_int("\n  Choice: ")) {
            case 1: place_order(&app); break;
            case 2: admin_panel(&app); break;
            case 3:
                puts("\n  Thank you for using our system. Goodbye!");
                running = 0;
                break;
            default: puts("  Please choose 1, 2, or 3.");
        }
    }

    return EXIT_SUCCESS;   /* no free() needed — all data lives on the stack */
}