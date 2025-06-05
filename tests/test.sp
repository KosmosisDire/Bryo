extern void Mycelium_String_print(string str);
extern void print_int(int val);

namespace ScopeTest
{
    class TestObject
    {
        public string name;
        public int id;

        ~TestObject()
        {
            Mycelium_String_print("DESTRUCTOR: " + name + " (id=" + id.ToString() + ")\n");
        }

        public TestObject(string name, int id)
        {
            this.name = name;
            this.id = id;
            Mycelium_String_print("CONSTRUCTOR: " + name + " (id=" + id.ToString() + ")\n");
        }
    }

    class Program
    {
        static int Main()
        {
            Mycelium_String_print("=== SCOPE & MEMORY MANAGEMENT TEST ===\n");

            // Test 1: String + Object in same scope (critical for memory corruption fix)
            Mycelium_String_print("\n--- Test 1: String + Object Mixed Scope ---\n");
            {
                string testStr = "Hello World";  // This should NOT be ARC-managed
                TestObject obj1 = new TestObject("ScopeTest1", 1);  // This SHOULD be ARC-managed
                Mycelium_String_print("String: " + testStr + "\n");
                Mycelium_String_print("Object: " + obj1.name + "\n");
                // Both testStr and obj1 should be cleaned up here
                // testStr should use string cleanup, obj1 should call destructor
            }
            Mycelium_String_print("After Test 1 scope exit\n");

            // Test 2: Loop with objects going out of scope
            Mycelium_String_print("\n--- Test 2: Loop Scope Cleanup ---\n");
            for (int i = 0; i < 3; i = i + 1)
            {
                TestObject loopObj = new TestObject("LoopObj", i);
                string loopStr = "Loop" + i.ToString();
                
                if (i == 1)
                {
                    TestObject innerObj = new TestObject("InnerObj", 99);
                    Mycelium_String_print("Created inner object\n");
                    // innerObj should be destroyed here when going out of scope
                }
                
                Mycelium_String_print("Loop iteration " + i.ToString() + " complete\n");
                // loopObj should be destroyed here at end of iteration
            }
            Mycelium_String_print("After loop complete\n");

            // Test 3: Early exit with continue
            Mycelium_String_print("\n--- Test 3: Continue Statement Cleanup ---\n");
            for (int j = 0; j < 3; j = j + 1)
            {
                TestObject continueObj = new TestObject("ContinueObj", j);
                
                if (j == 1)
                {
                    Mycelium_String_print("Continuing at j=" + j.ToString() + "\n");
                    continue; // continueObj should be destroyed before continue
                }
                
                Mycelium_String_print("Normal flow for j=" + j.ToString() + "\n");
                // continueObj should be destroyed here for j=0,2
            }
            Mycelium_String_print("After continue test\n");

            // Test 4: Early exit with break
            Mycelium_String_print("\n--- Test 4: Break Statement Cleanup ---\n");
            for (int k = 0; k < 5; k = k + 1)
            {
                TestObject breakObj = new TestObject("BreakObj", k);
                
                if (k == 2)
                {
                    Mycelium_String_print("Breaking at k=" + k.ToString() + "\n");
                    break; // breakObj should be destroyed before break
                }
                
                Mycelium_String_print("Normal flow for k=" + k.ToString() + "\n");
                // breakObj should be destroyed here for k=0,1
            }
            Mycelium_String_print("After break test\n");

            // Test 5: Nested blocks
            Mycelium_String_print("\n--- Test 5: Nested Block Scopes ---\n");
            {
                TestObject outer = new TestObject("OuterBlock", 100);
                {
                    TestObject inner = new TestObject("InnerBlock", 200);
                    {
                        TestObject deepest = new TestObject("DeepestBlock", 300);
                        Mycelium_String_print("All three objects created\n");
                        // deepest destroyed here
                    }
                    Mycelium_String_print("Deepest scope exited\n");
                    // inner destroyed here
                }
                Mycelium_String_print("Inner scope exited\n");
                // outer destroyed here
            }
            Mycelium_String_print("All nested scopes exited\n");

            // Test 6: Object assignment (ARC reference counting)
            Mycelium_String_print("\n--- Test 6: Object Assignment & ARC ---\n");
            TestObject original = new TestObject("Original", 500);
            TestObject copy = original; // Should increment ref count
            
            Mycelium_String_print("Original and copy point to same object\n");
            
            copy = new TestObject("NewObject", 600); // Should decrement ref count of original
            
            Mycelium_String_print("Copy now points to different object\n");
            // Both original and copy should be destroyed at function end

            Mycelium_String_print("\n=== TEST COMPLETE ===\n");
            Mycelium_String_print("Function ending - all remaining objects should be destroyed\n");
            
            return 0;
        }
    }
}
