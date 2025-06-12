// Test control flow statements

extern void Mycelium_String_print(string str);

class Program {
    static int Main() {
        Mycelium_String_print("=== Control Flow Test ===\n");
        
        // If-else test
        int x = 15;
        if (x > 10) {
            Mycelium_String_print("x is greater than 10\n");
        } else {
            Mycelium_String_print("x is not greater than 10\n");
        }
        
        // While loop test
        int counter = 0;
        while (counter < 3) {
            Mycelium_String_print("Counter: ");
            Mycelium_String_print((string)counter);
            Mycelium_String_print("\n");
            counter = counter + 1;
        }
        
        // For loop test
        Mycelium_String_print("For loop: ");
        for (int i = 0; i < 5; i = i + 1) {
            Mycelium_String_print((string)i);
            if (i < 4) {
                Mycelium_String_print(", ");
            }
        }
        Mycelium_String_print("\n");
        
        return 0;
    }
}