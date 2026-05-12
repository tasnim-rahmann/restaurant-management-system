/*
 * =============================================
 *   Food Ordering Management System
 * =============================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Limits ─────────────────────────────────────────────────── */
#define MAX_MENU      100
#define MAX_ORDERS    200
#define MAX_CARDS     200
#define MAX_USERS      50
#define NAME_LEN       50
#define PASS_LEN       32
#define ADMIN_PASS    "1234"

/* ══════════════════════════════════════════
   DATA STRUCTURES
═════════════════════════════════════════════ */

/* One food item on the menu */
typedef struct {
    int   serial;
    char  name[NAME_LEN];
    int   quantity;
    float price;
} FoodItem;

/* One item inside a customer's cart */
typedef struct {
    int food_serial;
    int quantity;
} CartItem;

/* One completed order record (for admin view) */
typedef struct {
    char username[NAME_LEN];
    int  food_serial;
    int  quantity;
    char payment_method[10];   /* "Cash" or "Card" */
} OrderRecord;

/* One card payment record */
typedef struct {
    int   card_number;
    float amount;
} CardPayment;

/* One registered user */
typedef struct {
    char username[NAME_LEN];
    char password[PASS_LEN];
} User;

/* The full application state — everything lives here */
typedef struct {
    /* Menu */
    FoodItem  menu[MAX_MENU];
    int       menu_count;

    /* Cart for the current logged-in session */
    CartItem  cart[MAX_ORDERS];
    int       cart_count;
    float     cart_total;

    /* Order history (for admin) */
    OrderRecord orders[MAX_ORDERS];
    int         order_count;

    /* Card payments */
    CardPayment card_payments[MAX_CARDS];
    int         card_count;

    /* Total cash earned today */
    float total_cash;

    /* Users */
    User users[MAX_USERS];
    int  user_count;

    /* Login state */
    int  logged_in;              /* 1 = someone is logged in, 0 = not */
    char current_user[NAME_LEN]; /* username of the logged-in user */
} AppState;


/* ══════════════════════════════════════════
   INPUT HELPERS
═════════════════════════════════════════════ */

/* Read one line from stdin safely */
static int read_line(char *buf, int max) {
    if (!fgets(buf, max, stdin)) return -1;
    buf[strcspn(buf, "\n")] = '\0';   /* remove trailing newline */
    return 0;
}

/* Keep asking until user types a valid integer */
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

/* Keep asking until user types a valid positive float */
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


/* ══════════════════════════════════════════
   MENU ARRAY HELPERS
═════════════════════════════════════════════ */

/* Add a food item to the menu array */
static int menu_add(AppState *app, int serial, const char *name, int qty, float price) {
    if (app->menu_count >= MAX_MENU) { puts("  Menu is full."); return 0; }
    FoodItem *f = &app->menu[app->menu_count++];
    f->serial   = serial;
    f->quantity = qty;
    f->price    = price;
    strncpy(f->name, name, NAME_LEN - 1);
    f->name[NAME_LEN - 1] = '\0';
    return 1;
}

/* Search menu array by serial number, return index or -1 */
static int menu_find_index(AppState *app, int serial) {
    for (int i = 0; i < app->menu_count; i++)
        if (app->menu[i].serial == serial) return i;
    return -1;
}

/* Return a pointer to the food item, or NULL if not found */
static FoodItem *menu_find(AppState *app, int serial) {
    int idx = menu_find_index(app, serial);
    return (idx >= 0) ? &app->menu[idx] : NULL;
}

/* Check if a serial number is already taken */
static int serial_exists(AppState *app, int serial) {
    return menu_find_index(app, serial) >= 0;
}

/* Delete item by shifting everything after it one slot left */
static int menu_delete(AppState *app, int serial) {
    int idx = menu_find_index(app, serial);
    if (idx < 0) return 0;   /* not found */
    for (int i = idx; i < app->menu_count - 1; i++)
        app->menu[i] = app->menu[i + 1];   /* shift left */
    app->menu_count--;
    return 1;
}


/* ══════════════════════════════════════════
   USER / AUTH HELPERS
═════════════════════════════════════════════ */

/* Find user index by username, return -1 if not found */
static int user_find(AppState *app, const char *username) {
    for (int i = 0; i < app->user_count; i++)
        if (strcmp(app->users[i].username, username) == 0) return i;
    return -1;
}

/* Register a new user */
static void auth_register(AppState *app) {
    if (app->user_count >= MAX_USERS) { puts("  User limit reached."); return; }

    char username[NAME_LEN], password[PASS_LEN];

    printf("  Username: ");
    read_line(username, sizeof username);
    if (username[0] == '\0') { puts("  Username cannot be empty."); return; }

    /* Check if username already taken */
    if (user_find(app, username) >= 0) {
        puts("  Username already exists. Please choose another.");
        return;
    }

    printf("  Password: ");
    read_line(password, sizeof password);
    if (password[0] == '\0') { puts("  Password cannot be empty."); return; }

    /* Save user to array */
    User *u = &app->users[app->user_count++];
    strncpy(u->username, username, NAME_LEN - 1);
    strncpy(u->password, password, PASS_LEN - 1);

    printf("  Registration successful! Welcome, %s.\n", username);
}

/* Log in an existing user — sets logged_in flag and current_user */
static void auth_login(AppState *app) {
    if (app->logged_in) {
        printf("  Already logged in as '%s'.\n", app->current_user);
        return;
    }

    char username[NAME_LEN], password[PASS_LEN];

    printf("  Username: ");
    read_line(username, sizeof username);
    printf("  Password: ");
    read_line(password, sizeof password);

    int idx = user_find(app, username);

    /* Check username exists and password matches */
    if (idx < 0 || strcmp(app->users[idx].password, password) != 0) {
        puts("  Incorrect username or password.");
        return;
    }

    /* Set login state */
    app->logged_in = 1;
    strncpy(app->current_user, username, NAME_LEN - 1);
    printf("  Login successful! Welcome back, %s.\n", username);
}

/* Log out the current user and clear their cart */
static void auth_logout(AppState *app) {
    if (!app->logged_in) { puts("  You are not logged in."); return; }
    printf("  Goodbye, %s!\n", app->current_user);
    app->logged_in       = 0;
    app->current_user[0] = '\0';
    /* Clear the cart on logout */
    app->cart_count = 0;
    app->cart_total = 0.0f;
}


/* ══════════════════════════════════════════
   DISPLAY HELPERS
═════════════════════════════════════════════ */

static void print_line(void) {
    puts("  +--------+--------------------------+-----------+----------+");
}

/* Print the full menu table */
static void print_food_list(AppState *app) {
    puts("\n  ===== Food Menu =====");
    print_line();
    printf("  | %-6s | %-24s | %-9s | %-8s |\n", "No.", "Name", "Price", "In Stock");
    print_line();
    for (int i = 0; i < app->menu_count; i++) {
        printf("  | %-6d | %-24s | %9.2f | %-8d |\n",
               app->menu[i].serial,
               app->menu[i].name,
               app->menu[i].price,
               app->menu[i].quantity);
    }
    print_line();
    putchar('\n');
}

/* Print the current cart */
static void print_cart(AppState *app) {
    if (app->cart_count == 0) { puts("  Your cart is empty."); return; }
    puts("\n  ===== Your Cart =====");
    puts("  +----------+--------------------------+-----------+-----------+");
    printf("  | %-8s | %-24s | %-9s | %-9s |\n", "Item No.", "Name", "Qty", "Subtotal");
    puts("  +----------+--------------------------+-----------+-----------+");
    for (int i = 0; i < app->cart_count; i++) {
        FoodItem *f = menu_find(app, app->cart[i].food_serial);
        if (!f) continue;
        float sub = f->price * app->cart[i].quantity;
        printf("  | %-8d | %-24s | %-9d | $%-8.2f |\n",
               i + 1, f->name, app->cart[i].quantity, sub);
    }
    puts("  +----------+--------------------------+-----------+-----------+");
    printf("  | %-45s | $%-8.2f |\n", "TOTAL", app->cart_total);
    puts("  +-----------------------------------------------+-----------+\n");
}

/* Print all completed orders (admin only) */
static void print_order_history(AppState *app) {
    if (app->order_count == 0) { puts("  No orders yet."); return; }
    puts("\n  ===== Order History =====");
    puts("  +----------+----------------+--------------------------+-----+--------+");
    printf("  | %-8s | %-14s | %-24s | %-3s | %-6s |\n",
           "Order No.", "User", "Item", "Qty", "Pay");
    puts("  +----------+----------------+--------------------------+-----+--------+");
    for (int i = 0; i < app->order_count; i++) {
        FoodItem *f = menu_find(app, app->orders[i].food_serial);
        const char *name = f ? f->name : "(deleted)";
        printf("  | %-8d | %-14s | %-24s | %-3d | %-6s |\n",
               i + 1,
               app->orders[i].username,
               name,
               app->orders[i].quantity,
               app->orders[i].payment_method);
    }
    puts("  +----------+----------------+--------------------------+-----+--------+\n");
}


/* ══════════════════════════════════════════
   CART & ORDERING
═════════════════════════════════════════════ */

/* Add one item to the cart (or increase quantity if already there) */
static void cart_add_item(AppState *app) {
    print_food_list(app);
    int serial = read_int("  Enter food number (0 to cancel): ");
    if (serial == 0) return;

    FoodItem *item = menu_find(app, serial);
    if (!item)               { puts("  Item not found."); return; }
    if (item->quantity == 0) { puts("  Sorry, this item is out of stock."); return; }

    printf("  %s — $%.2f each  (stock: %d)\n", item->name, item->price, item->quantity);
    int qty = read_int("  Quantity (0 to cancel): ");
    if (qty <= 0)            { puts("  Cancelled."); return; }
    if (qty > item->quantity){ puts("  Not enough stock."); return; }

    /* Check if item already in cart; if so just increase quantity */
    for (int i = 0; i < app->cart_count; i++) {
        if (app->cart[i].food_serial == serial) {
            app->cart[i].quantity += qty;
            app->cart_total       += item->price * qty;
            item->quantity        -= qty;           /* reserve stock */
            printf("  Updated cart: %s x%d\n", item->name, app->cart[i].quantity);
            return;
        }
    }

    /* New item in cart */
    if (app->cart_count >= MAX_ORDERS) { puts("  Cart is full."); return; }
    app->cart[app->cart_count].food_serial = serial;
    app->cart[app->cart_count].quantity    = qty;
    app->cart_count++;
    app->cart_total  += item->price * qty;
    item->quantity   -= qty;   /* reserve stock */

    printf("  Added to cart: %s x%d  ($%.2f)\n", item->name, qty, item->price * qty);
}

/* Process payment for everything in the cart */
static void checkout(AppState *app) {
    if (app->cart_count == 0) { puts("  Your cart is empty."); return; }

    print_cart(app);
    puts("  Payment method:  1. Cash   2. Credit Card   0. Cancel");
    int payment = read_int("  Choice: ");
    if (payment == 0) { puts("  Checkout cancelled."); return; }
    if (payment != 1 && payment != 2) { puts("  Invalid choice."); return; }

    char method[10];

    if (payment == 1) {
        strcpy(method, "Cash");
    } else {
        /* Credit card flow */
        if (app->card_count >= MAX_CARDS) { puts("  Card storage full."); return; }
        int card_no = read_int("  Enter card number: ");
        char pin[PASS_LEN];
        printf("  Enter card PIN: ");
        read_line(pin, sizeof pin);

        app->card_payments[app->card_count].card_number = card_no;
        app->card_payments[app->card_count].amount      = app->cart_total;
        app->card_count++;
        strcpy(method, "Card");
        puts("  Card payment recorded.");
    }

    /* Save each cart item as an order record */
    for (int i = 0; i < app->cart_count; i++) {
        if (app->order_count >= MAX_ORDERS) break;
        OrderRecord *rec = &app->orders[app->order_count++];
        strncpy(rec->username, app->current_user, NAME_LEN - 1);
        rec->food_serial = app->cart[i].food_serial;
        rec->quantity    = app->cart[i].quantity;
        strncpy(rec->payment_method, method, 9);
    }

    app->total_cash += app->cart_total;
    printf("\n  Payment successful! Total paid: $%.2f  Method: %s\n\n",
           app->cart_total, method);

    /* Clear cart */
    app->cart_count = 0;
    app->cart_total = 0.0f;
}

/* Cancel/clear the cart and return reserved stock */
static void cancel_order(AppState *app) {
    if (app->cart_count == 0) { puts("  Cart is already empty."); return; }

    /* Return reserved stock to menu */
    for (int i = 0; i < app->cart_count; i++) {
        FoodItem *f = menu_find(app, app->cart[i].food_serial);
        if (f) f->quantity += app->cart[i].quantity;
    }
    app->cart_count = 0;
    app->cart_total = 0.0f;
    puts("  Order cancelled. All items returned to stock.");
}

/*
* Main ordering loop for a logged-in user.
* After adding an item the user sees 3 options:
*   1. Add another item
*   2. Checkout (pay for everything)
*   3. Cancel order
*/

static void ordering_menu(AppState *app) {
    /* Must be logged in to order */
    if (!app->logged_in) {
        puts("  Please log in first to place an order.");
        return;
    }

    /* Add at least one item to start */
    cart_add_item(app);
    if (app->cart_count == 0) return;   /* user cancelled before adding anything */

    while (1) {
        print_cart(app);
        puts("  What would you like to do?");
        puts("  1. Add another item");
        puts("  2. Checkout (pay now)");
        puts("  3. Cancel order");

        int choice = read_int("  Choice: ");

        if (choice == 1) {
            cart_add_item(app);
        } else if (choice == 2) {
            checkout(app);
            return;   /* done ordering */
        } else if (choice == 3) {
            cancel_order(app);
            return;
        } else {
            puts("  Please choose 1, 2, or 3.");
        }
    }
}


/* ══════════════════════════════════════════
   ADMIN PANEL
═════════════════════════════════════════════ */

/* Admin: add a new food item */
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

    float price = read_float("  Price ($): ");
    if (menu_add(app, serial, name, qty, price))
        printf("  '%s' added to menu.\n", name);
}

/* Admin: delete a food item */
static void admin_delete_food(AppState *app) {
    print_food_list(app);
    int serial = read_int("  Enter serial to delete (0 to cancel): ");
    if (serial == 0) return;

    if (menu_delete(app, serial))
        printf("  Item #%d removed.\n", serial);
    else
        puts("  Item not found.");
}

/* Admin: save a backup text file */
static void admin_backup(AppState *app) {
    time_t     now = time(NULL);
    struct tm *t   = localtime(&now);
    char filename[64];
    strftime(filename, sizeof filename, "backup_%Y-%m-%d.txt", t);

    FILE *f = fopen(filename, "w");
    if (!f) { perror("  Cannot create file"); return; }

    char datebuf[64];
    strftime(datebuf, sizeof datebuf, "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "=== Food Ordering System Backup ===\nDate: %s\n\n", datebuf);
    fprintf(f, "Total cash today: $%.2f\n\n", app->total_cash);

    /* Card payments */
    if (app->card_count > 0) {
        fprintf(f, "Card Payments:\n");
        for (int i = 0; i < app->card_count; i++)
            fprintf(f, "  Card %d  $%.2f\n",
                    app->card_payments[i].card_number,
                    app->card_payments[i].amount);
        fputc('\n', f);
    }

    /* Food inventory */
    fprintf(f, "Food Inventory:\n");
    for (int i = 0; i < app->menu_count; i++)
        fprintf(f, "  [%d] %-24s qty:%-4d  $%.2f\n",
                app->menu[i].serial, app->menu[i].name,
                app->menu[i].quantity, app->menu[i].price);

    fclose(f);
    printf("  Backup saved as '%s'\n", filename);
}

/* Admin panel — password protected */
static void admin_panel(AppState *app) {
    char pass[PASS_LEN];
    printf("  Admin Password: ");
    if (read_line(pass, sizeof pass) < 0) return;

    if (strcmp(pass, ADMIN_PASS) != 0) {
        puts("  Incorrect password.");
        return;
    }

    int running = 1;
    while (running) {
        puts("\n  ===== Admin Panel =====");
        puts("  1. Total cash today");
        puts("  2. View card payments");
        puts("  3. Add food item");
        puts("  4. Delete food item");
        puts("  5. View food menu");
        puts("  6. Total menu items");
        puts("  7. Backup data");
        puts("  8. View order history");
        puts("  9. View registered users");
        puts("  0. Back to main menu");

        switch (read_int("\n  Choice: ")) {
            case 1:
                printf("\n  Today's total cash: $%.2f\n", app->total_cash);
                break;

            case 2:
                if (app->card_count == 0) { puts("  No card payments."); break; }
                puts("\n  Card Payments:");
                for (int i = 0; i < app->card_count; i++)
                    printf("    Card %-10d  $%.2f\n",
                           app->card_payments[i].card_number,
                           app->card_payments[i].amount);
                break;

            case 3: admin_add_food(app);   break;
            case 4: admin_delete_food(app); break;
            case 5: print_food_list(app);   break;

            case 6:
                printf("\n  Total menu items: %d\n", app->menu_count);
                break;

            case 7: admin_backup(app); break;
            case 8: print_order_history(app); break;

            case 9:
                puts("\n  Registered Users:");
                if (app->user_count == 0) { puts("  None."); break; }
                for (int i = 0; i < app->user_count; i++)
                    printf("    %d. %s\n", i + 1, app->users[i].username);
                break;

            case 0: running = 0; break;
            default: puts("  Please choose from the list.");
        }
    }
}


/* ══════════════════════════════════════════
   MAIN
═════════════════════════════════════════════ */
int main(void) {
    AppState app = {0};

    menu_add(&app, 1, "Burger",      23,  175.00f);
    menu_add(&app, 2, "Pizza",       13,  250.00f);
    menu_add(&app, 3, "Hot Cake",     8,  475.00f);
    menu_add(&app, 4, "Coffee",      46,  325.00f);
    menu_add(&app, 5, "Ice Cream",   46,  230.00f);
    menu_add(&app, 6, "Sandwich",    34,  320.00f);
    menu_add(&app, 7, "Grill",        7,  100.00f);
    menu_add(&app, 8, "Pancakes",   121,  350.00f);
    menu_add(&app, 9, "Cold Drinks", 73,  120.00f);

    puts("==========================================");
    puts("    FOOD ORDERING MANAGEMENT SYSTEM");
    puts("==========================================\n");

    int running = 1;
    while (running) {
        /* Show login status in the menu */
        if (app.logged_in)
            printf("  Logged in as: %s\n", app.current_user);
        else
            puts("  Not logged in");

        puts("\n  ===== Main Menu =====");
        puts("  1. Register");
        puts("  2. Login");
        puts("  3. Place an order  (login required)");
        puts("  4. View menu");
        puts("  5. Logout");
        puts("  6. Admin panel");
        puts("  0. Exit");

        switch (read_int("\n  Choice: ")) {
            case 1: auth_register(&app); break;
            case 2: auth_login(&app);    break;
            case 3: ordering_menu(&app); break;
            case 4: print_food_list(&app); break;
            case 5: auth_logout(&app);   break;
            case 6: admin_panel(&app);   break;
            case 0:
                puts("\n  Thank you! Goodbye.");
                running = 0;
                break;
            default:
                puts("  Please choose a valid option.");
        }
    }

    return EXIT_SUCCESS;
}