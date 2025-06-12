// Test forward declarations and mutual dependencies

extern void Mycelium_String_print(string str);

class ClassA {
    static int processValue(int x) {
        Mycelium_String_print("ClassA processing value\n");
        if (x > 50) {
            return x;
        }
        // Forward call to ClassB
        return ClassB.transformValue(x * 2);
    }
    
    static string getPrefix() {
        return "A-";
    }
}

class ClassB {
    static int transformValue(int y) {
        Mycelium_String_print("ClassB transforming value\n");
        if (y > 100) {
            return y;
        }
        // Call back to ClassA
        string prefix = ClassA.getPrefix();
        Mycelium_String_print(prefix + "transformed\n");
        return y + 10;
    }
}

class Program {
    static int Main() {
        Mycelium_String_print("=== Forward Declarations Test ===\n");
        
        int result1 = ClassA.processValue(25);
        Mycelium_String_print((string)result1);
        Mycelium_String_print("\n");
        
        int result2 = ClassB.transformValue(75);
        Mycelium_String_print((string)result2);
        Mycelium_String_print("\n");
        
        return 0;
    }
}