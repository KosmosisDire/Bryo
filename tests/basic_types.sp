// Test basic types and operations

extern void Mycelium_String_print(string str);

class Program {
    static int Main() {
        Mycelium_String_print("=== Basic Types Test ===\n");
        
        // Integer operations
        int a = 10;
        int b = 20;
        int sum = a + b;
        Mycelium_String_print((string)sum);
        Mycelium_String_print("\n");
        
        // Boolean operations
        bool flag = true;
        bool result = flag;
        Mycelium_String_print((string)result);
        Mycelium_String_print("\n");
        
        // String operations
        string greeting = "Hello";
        string name = "World";
        string message = greeting + ", " + name + "!";
        Mycelium_String_print(message + "\n");
        
        return 0;
    }
}