// UML Diagram Generation Test
// This file tests the enhanced semantic analyzer UML output generation
// Focuses on features that are currently supported

extern void Mycelium_String_print(string str);
extern void print_int(int val);

// Test 1: Forward declaration class for UML visualization
class ForwardTestClass
{
    int value;
    string name;
    
    ForwardTestClass(int v)
    {
        value = v;
        name = "ForwardTest";
    }
    
    // Forward declaration test - method_a calls method_b defined later
    static int method_a(int n)
    {
        if (n <= 0) return 0;
        return method_b(n - 1);  // Forward reference
    }
    
    // method_b defined after method_a (mutual recursion)
    static int method_b(int n)
    {
        if (n <= 0) return 42;
        return n + method_a(n - 1);
    }
    
    static int get_static_value()
    {
        return 42;
    }
    
    static int test_forward_calls()
    {
        int result = method_a(3);
        print_int(result);
        return result;
    }
}

// Test 2: Service class (should get utility styling)
class DataService
{
    int service_id;
    string service_name;
    
    DataService(int id, string name)
    {
        service_id = id;
        service_name = name;
    }
    
    static void process_data(int data_value)
    {
        print_int(data_value);
        Mycelium_String_print("Data processed\n");
    }
    
    static int get_default_service_id()
    {
        return 100;
    }
    
    static void perform_static_service_operation()
    {
        int default_id = get_default_service_id();
        process_data(default_id);
    }
}

// Test 3: Mock class (should get test styling)
class MockTestHelper
{
    bool is_mocked;
    int mock_value;
    
    MockTestHelper(bool mocked)
    {
        is_mocked = mocked;
        mock_value = 999;
    }
    
    static int mock_method(int input)
    {
        return input * 2;
    }
    
    static bool get_default_mock_status()
    {
        return true;
    }
    
    static int get_default_mock_value()
    {
        return 999;
    }
}

// Test 4: Cross-class forward references
class ClassA
{
    int data_a;
    
    ClassA(int value)
    {
        data_a = value;
    }
    
    // This method calls a method in ClassB (forward reference)
    static int call_class_b(int param)
    {
        return ClassB.method_from_b(param);
    }
    
    static int get_static_data()
    {
        return 123;
    }
}

// ClassB defined after ClassA but referenced by ClassA
class ClassB
{
    string data_b;
    
    ClassB(string value)
    {
        data_b = value;
    }
    
    // This method calls back to ClassA (mutual cross-class reference)
    static int method_from_b(int param)
    {
        if (param <= 0) return 0;
        
        int class_a_data = ClassA.get_static_data();
        return class_a_data + 10;
    }
    
    // Method that uses ClassA
    static void test_cross_class()
    {
        int result = ClassA.call_class_b(5);
        print_int(result);
    }
}

// Test 5: Chain of forward declarations (circular dependencies)
class ChainA
{
    // Calls ChainB
    static int chain_call_1(int n)
    {
        if (n <= 0) return 1;
        return ChainB.chain_call_2(n - 1);
    }
}

class ChainB
{
    // Calls ChainC
    static int chain_call_2(int n)
    {
        if (n <= 0) return 2;
        return ChainC.chain_call_3(n - 1);
    }
}

class ChainC
{
    // Calls back to ChainA (circular chain)
    static int chain_call_3(int n)
    {
        if (n <= 0) return 3;
        return ChainA.chain_call_1(n - 1) + 1;
    }
}

// Test 6: Resource management class
class ResourceManager
{
    int resource_id;
    bool is_initialized;
    
    ResourceManager(int id)
    {
        resource_id = id;
        is_initialized = false;
        initialize_resource(id);  // Forward reference to static method
    }
    
    ~ResourceManager()
    {
        cleanup_resource(resource_id);  // Forward reference to static method
    }
    
    // Static methods defined after constructor/destructor
    static void initialize_resource(int id)
    {
        print_int(id);
        Mycelium_String_print("Resource initialized\n");
    }
    
    static void cleanup_resource(int id)
    {
        print_int(id);
        Mycelium_String_print("Resource cleaned up\n");
    }
    
    static int get_default_resource_id()
    {
        return 42;
    }
    
    static void set_default_initialized()
    {
        bool init_status = true;
        print_int(1);
    }
}

// Test 7: Comprehensive test class with multiple types
class ComprehensiveTestClass
{
    int int_field;
    string string_field;
    bool bool_field;
    
    ComprehensiveTestClass(int i, string s, bool b)
    {
        int_field = i;
        string_field = s;
        bool_field = b;
    }
    
    static void static_helper_method(int param)
    {
        print_int(param);
        Mycelium_String_print("Static helper called\n");
    }
    
    static int calculate_result(int param)
    {
        static_helper_method(param);
        return param + 100;
    }
    
    static void test_static_scope_variables()
    {
        int local_var = 10;
        string local_string = "local";
        
        if (local_var > 5)
        {
            int nested_var = local_var + 5;
            print_int(nested_var);
        }
        
        while (local_var > 0)
        {
            int loop_var = local_var * 2;
            print_int(loop_var);
            local_var = local_var - 1;
            
            if (local_var <= 5) break;
        }
        
        for (int i = 0; i < 3; i = i + 1)
        {
            int for_var = i * 10;
            print_int(for_var);
            
            if (for_var > 10) continue;
        }
    }
}

// Test 8: Main class (should get entry point styling)
class Main
{
    static int Main()
    {
        Mycelium_String_print("=== UML Generation Test ===\n");
        
        // Test basic forward declarations
        Mycelium_String_print("Testing forward declarations...\n");
        int forward_result = ForwardTestClass.test_forward_calls();
        
        // Test cross-class references
        Mycelium_String_print("Testing cross-class references...\n");
        ClassB.test_cross_class();
        
        // Test circular chain
        Mycelium_String_print("Testing circular chain...\n");
        int chain_result = ChainA.chain_call_1(3);
        print_int(chain_result);
        
        // Test service class
        Mycelium_String_print("Testing service class...\n");
        DataService.perform_static_service_operation();
        
        // Test mock class
        Mycelium_String_print("Testing mock class...\n");
        int mock_result = MockTestHelper.get_default_mock_value();
        print_int(mock_result);
        
        // Test resource management
        Mycelium_String_print("Testing resource management...\n");
        int resource_id = ResourceManager.get_default_resource_id();
        print_int(resource_id);
        
        // Test comprehensive class
        Mycelium_String_print("Testing comprehensive class...\n");
        int comp_result = ComprehensiveTestClass.calculate_result(50);
        print_int(comp_result);
        ComprehensiveTestClass.test_static_scope_variables();
        
        Mycelium_String_print("=== UML Generation Test Complete ===\n");
        return 0;
    }
}