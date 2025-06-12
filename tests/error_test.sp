// Test that should fail - for testing error handling

extern void Mycelium_String_print(string str);

class Program {
    static int Main() {
        Mycelium_String_print("=== Error Test (Should Fail) ===\n");
        
        // This should cause a compilation error - undefined variable
        int x = undefined_variable;
        Mycelium_String_print((string)x);
        
        return 0;
    }
}