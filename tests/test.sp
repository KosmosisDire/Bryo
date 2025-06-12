// ============================================================================
// Comprehensive Semantic Analysis Test Suite
//
// This file is designed to test the limits of the Sharpie semantic analyzer,
// including its multi-pass forward declaration resolution, type checking,
// scope management, and usage graph generation for the Semantic IR.
// ============================================================================

// --- External Function Declarations ---
// These are required for the test functions to print output.

extern void Mycelium_String_print(string str);
extern void print_int(int val);
extern void print_bool(bool val);


// ============================================================================
// TEST 1: Basic Scopes, Variables, and Control Flow
// ============================================================================
class ScopeTester {
    static void testAllScopes() {
        Mycelium_String_print("--- Test 1: Scope and Variable Test ---\n");
        
        int outerVar = 10;
        
        if (outerVar > 5) {
            string innerVar = "Hello from if-block";
            Mycelium_String_print(innerVar + "\n");
            outerVar = 15; // Modifying outer scope variable is allowed
        }
        
        // ERROR: innerVar is not defined here. Uncommenting should cause a semantic error.
        // Mycelium_String_print(innerVar); 

        while (outerVar > 10) {
            bool loopVar = true;
            print_bool(loopVar);
            outerVar = outerVar - 1;
        }

        // ERROR: loopVar is not defined here.
        // print_bool(loopVar);
    }
}


// ============================================================================
// TEST 2: Class Members, `this` Keyword, and Constructor Forward Calls
// ============================================================================
class InstanceTester {
    int id;
    string name;
    bool is_initialized;

    // Constructor calls a private method defined later (constructor forward call)
    InstanceTester(int id, string name) {
        this.id = id;
        this.name = name;
        this.initialize(); // Forward call to instance method
    }

    // Instance method using both explicit and implicit `this`
    void printDetails() {
        Mycelium_String_print("Instance ID: ");
        print_int(this.id);
        Mycelium_String_print(", Name: " + name + "\n"); // `name` is an implicit `this.name`
    }
    
    // A static method has no `this` context
    static InstanceTester createDefault() {
        Mycelium_String_print("Creating default instance...\n");
        return new InstanceTester(0, "Default");

        // ERROR: A static method cannot use `this`. Uncommenting should fail.
        // this.id = 5; 
    }
    
    // Private helper method defined after its use in the constructor
    private void initialize() {
        this.is_initialized = true;
        Mycelium_String_print("InstanceTester Initialized.\n");
    }
}


// ============================================================================
// TEST 3: Advanced Intra-Class Forward Declarations (Mutual Recursion)
// ============================================================================
class RecursiveTester {
    // isEven calls isOdd (forward reference)
    static bool isEven(int n) {
        if (n == 0) return true;
        return isOdd(n - 1);
    }

    // isOdd calls isEven (mutual recursion)
    static bool isOdd(int n) {
        if (n == 0) return false;
        return isEven(n - 1);
    }
}


// ============================================================================
// TEST 4: Cross-Class Forward Declarations
// ServiceA is defined first but depends on ServiceB.
// ============================================================================
class ServiceA {
    // This method makes a forward call to a method in ServiceB
    static string processRequest(string input) {
        Mycelium_String_print("ServiceA: Received request, forwarding to ServiceB...\n");
        // Forward call: ServiceB is not fully defined yet.
        return ServiceB.handleRequest(input); 
    }

    // This method is called by ServiceB, creating a mutual dependency.
    static string getUtilityValue() {
        return " (processed by A)";
    }
}

// ServiceB is defined after ServiceA.
class ServiceB {
    // This method is called by ServiceA.
    static string handleRequest(string data) {
        Mycelium_String_print("ServiceB: Handling request.\n");
        string result = "Response from B for: " + data;
        
        // This is a normal (not forward) call back to ServiceA.
        return result + ServiceA.getUtilityValue();
    }
}


// ============================================================================
// TEST 5: Circular Dependency Chain (A -> B -> C -> A)
// This tests the analyzer's ability to handle cycles in the dependency graph.
// ============================================================================
class ChainLinkA {
    static int startChain(int val) {
        Mycelium_String_print("Chain A -> B\n");
        return ChainLinkB.continueChain(val + 1); // Forward call
    }
}

class ChainLinkB {
    static int continueChain(int val) {
        Mycelium_String_print("Chain B -> C\n");
        return ChainLinkC.finishChain(val * 2); // Forward call
    }
}

class ChainLinkC {
    static int finishChain(int val) {
        Mycelium_String_print("Chain C -> A (loop)\n");
        if (val > 100) return val;
        // Circular forward call back to the start of the chain.
        return ChainLinkA.startChain(val); 
    }
}


// ============================================================================
// TEST 6: Namespaces and Cross-Namespace Dependencies
// ============================================================================
namespace MyCompany.Services {
    class InternalLogger {
        static void log(string message) {
            Mycelium_String_print("[LOG]: " + message + "\n");
        }
    }
}

// A class outside the namespace that depends on a class inside it.
class ExternalClient {
    static void performAction() {
        Mycelium_String_print("ExternalClient: Performing action, will log via internal service.\n");
        // This should create a dependency from ExternalClient to InternalLogger.
        MyCompany.Services.InternalLogger.log("Action performed by ExternalClient");
    }
}


// ============================================================================
// TEST 7: Main orchestrator class
// This class calls all other test classes to build the full usage graph.
// ============================================================================
class Program {
    static int Main() {
        Mycelium_String_print("=== Comprehensive Semantic Test Suite Starting ===\n\n");

        // Test 1
        ScopeTester.testAllScopes();

        // Test 2
        Mycelium_String_print("\n--- Test 2: Instance and `this` Test ---\n");
        InstanceTester tester = new InstanceTester(101, "MyInstance");
        tester.printDetails();
        InstanceTester defaultTester = InstanceTester.createDefault();
        defaultTester.printDetails();

        // Test 3
        Mycelium_String_print("\n--- Test 3: Mutual Recursion Test ---\n");
        bool evenResult = RecursiveTester.isEven(10);
        Mycelium_String_print("Is 10 even? "); print_bool(evenResult); Mycelium_String_print("\n");
        bool oddResult = RecursiveTester.isOdd(7);
        Mycelium_String_print("Is 7 odd? "); print_bool(oddResult); Mycelium_String_print("\n");

        // Test 4
        Mycelium_String_print("\n--- Test 4: Cross-Class Forward Call Test ---\n");
        string response = ServiceA.processRequest("MyData");
        Mycelium_String_print("Final Response: " + response + "\n");

        // Test 5
        Mycelium_String_print("\n--- Test 5: Circular Dependency Test ---\n");
        int chainResult = ChainLinkA.startChain(1);
        Mycelium_String_print("Circular chain ended with value: "); print_int(chainResult); Mycelium_String_print("\n");

        // Test 6
        Mycelium_String_print("\n--- Test 6: Namespace Test ---\n");
        ExternalClient.performAction();

        Mycelium_String_print("\n=== Comprehensive Semantic Test Suite Complete ===\n");
        return 0;
    }
}


// ============================================================================
// TEST 8: Error Condition Tests (Commented Out)
// Uncommenting any of these lines should cause a specific semantic error.
// ============================================================================
/*
class ErrorTester {
    int instanceField = 1;

    static void triggerErrors() {
        // --- Type Mismatch Error ---
        int a = "this is not an int";

        // --- Undefined Variable Error ---
        int b = c; 

        // --- Wrong Argument Count Error ---
        print_int(1, 2, 3);

        // --- Wrong Argument Type Error ---
        print_int("this is not an int");

        // --- Accessing Instance Member from Static Context ---
        int d = instanceField;

        // --- Calling Method on Non-Class Type ---
        int e = 10;
        e.ToString(); // Assuming methods on primitives are not yet supported this way
    }
}
*/