extern void Mycelium_String_print(string str);
extern void print_int(int val);

namespace EdgeCaseTests
{
    // Simple data class for testing
    class DataItem
    {
        public string value;
        public int priority;
        public bool isActive;

        ~DataItem()
        {
            Mycelium_String_print("DataItem destructor called for: " + value + " (priority: " + priority.ToString() + ")\n");
        }

        public DataItem(string value, int priority)
        {
            this.value = value;
            this.priority = priority;
            this.isActive = true;
            Mycelium_String_print("DataItem constructor called for: " + value + " (priority: " + priority.ToString() + ")\n");
        }

        public string GetInfo()
        {
            return value + "[" + priority.ToString() + "]" + (isActive ? "*" : "");
        }
    }

    // Test class for complex nesting without circular references
    class Container
    {
        public string name;
        public DataItem primaryItem;
        public DataItem secondaryItem;
        public bool isValid;

        ~Container()
        {
            Mycelium_String_print("Container destructor called for: " + name + "\n");
        }

        public Container(string name)
        {
            this.name = name;
            this.isValid = true;
            Mycelium_String_print("Container constructor called for: " + name + "\n");
        }

        public void SetPrimary(string value, int priority)
        {
            primaryItem = new DataItem(value, priority);
        }

        public void SetSecondary(string value, int priority)
        {
            secondaryItem = new DataItem(value, priority);
        }

        public string GetSummary()
        {
            string summary = name + ": ";
            if (primaryItem != primaryItem) // Simple null check equivalent
            {
                summary = summary + "Primary=" + primaryItem.GetInfo();
            }
            return summary;
        }
    }

    // Test class for method chaining
    class Builder
    {
        public string result;
        public int count;

        ~Builder()
        {
            Mycelium_String_print("Builder destructor called with result: " + result + "\n");
        }

        public Builder()
        {
            this.result = "";
            this.count = 0;
            Mycelium_String_print("Builder constructor called\n");
        }

        public Builder Append(string text)
        {
            result = result + text;
            count = count + 1;
            return this; // Return self for chaining
        }

        public string Build()
        {
            return "Built(" + count.ToString() + "): " + result;
        }
    }

    class EdgeTestProgram
    {
        static int Main()
        {
            Mycelium_String_print("=== COMPREHENSIVE EDGE CASE TESTS ===\n\n");

            // Test 1: Deep Nesting with Early Returns
            Mycelium_String_print("--- Test 1: Deep Nesting & Early Returns ---\n");
            for (int i = 0; i < 3; i = i + 1)
            {
                Container root = new Container("Root" + i.ToString());
                
                for (int j = 0; j < 2; j = j + 1)
                {
                    if (j == 1 && i == 1)
                    {
                        Mycelium_String_print("Early break at i=" + i.ToString() + ", j=" + j.ToString() + "\n");
                        break; // Should clean up current objects
                    }
                    
                    DataItem rootData = new DataItem("RootData" + i.ToString(), i * 100);
                    root.primaryItem = rootData;
                    
                    for (int k = 0; k < 2; k = k + 1)
                    {
                        if (k == 1 && i == 0)
                        {
                            DataItem tempData = new DataItem("TempData" + k.ToString(), k * 100);
                            root.secondaryItem = tempData;
                            continue; // Should clean up tempData properly
                        }
                        
                        DataItem loopData = new DataItem("LoopData" + k.ToString(), k + i * 10 + j);
                        // loopData should be cleaned up at end of iteration
                    }
                }
                // root and all contained items should be cleaned up here
            }

            // Test 2: Complex Object Graphs
            Mycelium_String_print("\n--- Test 2: Complex Object Graphs ---\n");
            Container mainContainer = new Container("MainContainer");
            
            // Create a network of containers with data items
            for (int level = 0; level < 3; level = level + 1)
            {
                Container levelContainer = new Container("Level" + level.ToString());
                
                // Add multiple data items to each level
                for (int itemIdx = 0; itemIdx < 4; itemIdx = itemIdx + 1)
                {
                    if (itemIdx == 2) continue; // Skip some to test continue in loops
                    
                    string itemData = "L" + level.ToString() + "I" + itemIdx.ToString();
                    DataItem levelItem = new DataItem(itemData, level * 100 + itemIdx);
                    
                    // Create temporary containers in inner scope
                    if (itemIdx == 1)
                    {
                        Container tempContainer = new Container("Temp" + level.ToString());
                        DataItem tempItem = new DataItem("TempData", -999);
                        tempContainer.primaryItem = tempItem;
                        // tempContainer and tempItem should be cleaned up here
                    }
                    
                    levelContainer.primaryItem = levelItem;
                }
                // levelContainer should be cleaned up here
            }

            // Test 3: Builder Pattern with Method Chaining
            Mycelium_String_print("\n--- Test 3: Builder Pattern & Method Chaining ---\n");
            Builder builder1 = new Builder();
            Builder result1 = builder1.Append("Hello").Append(" ").Append("World");
            Mycelium_String_print("Result 1: " + result1.Build() + "\n");
            
            // Test builder in loop with reassignment
            for (int builderTest = 0; builderTest < 2; builderTest = builderTest + 1)
            {
                Builder loopBuilder = new Builder();
                loopBuilder = loopBuilder.Append("Loop").Append(builderTest.ToString());
                
                if (builderTest == 0)
                {
                    Builder innerBuilder = new Builder();
                    innerBuilder = innerBuilder.Append("Inner").Append("Test");
                    Mycelium_String_print("Inner: " + innerBuilder.Build() + "\n");
                    // innerBuilder should be cleaned up here
                }
                
                Mycelium_String_print("Loop: " + loopBuilder.Build() + "\n");
                // loopBuilder should be cleaned up here
            }

            // Test 4: Stress Test with Many Objects
            Mycelium_String_print("\n--- Test 4: Stress Test (Many Objects) ---\n");
            for (int stressOuter = 0; stressOuter < 5; stressOuter = stressOuter + 1)
            {
                Container stressContainer = new Container("Stress" + stressOuter.ToString());
                
                for (int stressInner = 0; stressInner < 3; stressInner = stressInner + 1)
                {
                    DataItem stressData = new DataItem("StressData" + stressInner.ToString(), stressOuter * 1000 + stressInner);
                    Builder stressBuilder = new Builder();
                    stressBuilder = stressBuilder.Append("Stress").Append(stressOuter.ToString()).Append("_").Append(stressInner.ToString());
                    
                    // Create nested temporary objects
                    for (int stressMicro = 0; stressMicro < 2; stressMicro = stressMicro + 1)
                    {
                        DataItem microData = new DataItem("Micro" + stressMicro.ToString(), stressMicro);
                        Container microContainer = new Container("MicroContainer" + stressMicro.ToString());
                        microContainer.primaryItem = microData;
                        
                        if (stressMicro == 0)
                        {
                            Builder microBuilder = new Builder();
                            microBuilder = microBuilder.Append("Micro").Append("Test");
                            // microBuilder cleaned up here
                        }
                        // microData and microContainer cleaned up here
                    }
                    
                    // stressData and stressBuilder cleaned up here
                }
                // stressContainer cleaned up here
            }

            // Test 5: Assignment Chain Testing
            Mycelium_String_print("\n--- Test 5: Assignment Chains ---\n");
            DataItem originalData = new DataItem("Original", 1);
            DataItem copyData = originalData; // Should increase ref count
            DataItem anotherCopy = copyData;  // Should increase ref count again
            
            Mycelium_String_print("Original: " + originalData.value + "\n");
            Mycelium_String_print("Copy: " + copyData.value + "\n");
            Mycelium_String_print("Another: " + anotherCopy.value + "\n");
            
            // Reassign to break references
            copyData = new DataItem("NewCopy", 2);
            anotherCopy = new DataItem("AnotherNew", 3);
            
            Mycelium_String_print("After reassignment:\n");
            Mycelium_String_print("Original: " + originalData.value + "\n");
            Mycelium_String_print("Copy: " + copyData.value + "\n");
            Mycelium_String_print("Another: " + anotherCopy.value + "\n");

            // Test 6: Exception-like Early Returns
            Mycelium_String_print("\n--- Test 6: Early Returns & Complex Control Flow ---\n");
            for (int errorTest = 0; errorTest < 4; errorTest = errorTest + 1)
            {
                Container errorContainer = new Container("ErrorTest" + errorTest.ToString());
                DataItem errorData = new DataItem("ErrorData", errorTest);
                
                if (errorTest == 1)
                {
                    Mycelium_String_print("Simulating error condition at " + errorTest.ToString() + "\n");
                    // Objects should be cleaned up before continue
                    continue;
                }
                
                if (errorTest == 3)
                {
                    Mycelium_String_print("Simulating critical error at " + errorTest.ToString() + "\n");
                    // Objects should be cleaned up before break
                    break;
                }
                
                Builder errorBuilder = new Builder();
                errorBuilder = errorBuilder.Append("Error").Append("Handled").Append(errorTest.ToString());
                Mycelium_String_print("Success: " + errorBuilder.Build() + "\n");
                
                // All objects cleaned up here in normal case
            }

            Mycelium_String_print("\n=== END OF EDGE CASE TESTS ===\n");
            Mycelium_String_print("All objects should now be destroyed...\n\n");

            return 0;
        }
    }
}
