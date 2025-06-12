// Test basic class functionality

extern void Mycelium_String_print(string str);

class SimpleClass {
    int value;
    string name;
    
    SimpleClass(int v, string n) {
        this.value = v;
        this.name = n;
    }
    
    void printInfo() {
        Mycelium_String_print("Name: " + this.name + ", Value: ");
        Mycelium_String_print((string)this.value);
        Mycelium_String_print("\n");
    }
    
    int getValue() {
        return this.value;
    }
    
    static SimpleClass createDefault() {
        return new SimpleClass(42, "Default");
    }
}

class Program {
    static int Main() {
        Mycelium_String_print("=== Class Basics Test ===\n");
        
        // Create instance with constructor
        SimpleClass obj1 = new SimpleClass(100, "TestObject");
        obj1.printInfo();
        
        // Test method calls
        int val = obj1.getValue();
        Mycelium_String_print("Retrieved value: ");
        Mycelium_String_print((string)val);
        Mycelium_String_print("\n");
        
        // Test static method
        SimpleClass obj2 = SimpleClass.createDefault();
        obj2.printInfo();
        
        return 0;
    }
}