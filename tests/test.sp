extern void Mycelium_String_print(string str);
extern void print_int(int val);

namespace ComprehensiveTest
{
    // Test class with various field types
    class Person
    {
        public string name;
        public int age;
        public bool isActive;
        public float height;

        ~Person()
        {
            Mycelium_String_print("Person destructor called for: " + name + "\n");
        }

        public Person(string name, int age, bool isActive, float height)
        {
            this.name = name;
            this.age = age;
            this.isActive = isActive;
            this.height = height;
            Mycelium_String_print("Person constructor called for: " + name + "\n");
        }

        public string GetInfo()
        {
            string info = "Person: " + name + ", Age: " + age.ToString() + 
                         ", Active: " + isActive.ToString() + 
                         ", Height: " + height.ToString();
            return info;
        }
    }

    // Test class with nested object references
    class Company
    {
        public string companyName;
        public Person ceo;
        public int employeeCount;

        ~Company()
        {
            Mycelium_String_print("Company destructor called for: " + companyName + "\n");
        }

        public Company(string name, Person ceo, int count)
        {
            this.companyName = name;
            this.ceo = ceo;
            this.employeeCount = count;
            Mycelium_String_print("Company constructor called for: " + name + "\n");
        }

        public string GetCompanyInfo()
        {
            return "Company: " + companyName + ", CEO: " + ceo.name + 
                   ", Employees: " + employeeCount.ToString();
        }
    }

    // Test class with primitive operations
    class Calculator
    {
        public int result;

        ~Calculator()
        {
            Mycelium_String_print("Calculator destructor called\n");
        }

        public Calculator()
        {
            this.result = 0;
            Mycelium_String_print("Calculator constructor called\n");
        }

        public int Add(int a, int b)
        {
            result = a + b;
            return result;
        }

        public int Multiply(int a, int b)
        {
            result = a * b;
            return result;
        }

        public string GetResultString()
        {
            return "Result: " + result.ToString();
        }
    }

    class Program
    {
        static int Main()
        {
            Mycelium_String_print("=== COMPREHENSIVE SHARPIE LANGUAGE TEST ===\n\n");

            // Test 1: Primitive Types and Method Calls
            Mycelium_String_print("--- Test 1: Primitive Types ---\n");
            int testInt = 42;
            bool testBool = true;
            float testFloat = 3.14f;
            string testString = "Hello Sharpie";
            
            Mycelium_String_print("Int: " + testInt.ToString() + "\n");
            Mycelium_String_print("Bool: " + testBool.ToString() + "\n");
            Mycelium_String_print("Float: " + testFloat.ToString() + "\n");
            Mycelium_String_print("String: " + testString + "\n");
            Mycelium_String_print("String length: " + testString.get_Length().ToString() + "\n");

            // Test 2: Object Creation and ARC/VTable Testing
            Mycelium_String_print("\n--- Test 2: Object Creation (VTable Debug) ---\n");
            Person alice = new Person("Alice Johnson", 30, true, 5.6f);
            Person bob = new Person("Bob Smith", 45, false, 6.0f);
            
            Mycelium_String_print(alice.GetInfo() + "\n");
            Mycelium_String_print(bob.GetInfo() + "\n");

            // Test 3: Nested Object References
            Mycelium_String_print("\n--- Test 3: Nested Objects ---\n");
            Company techCorp = new Company("TechCorp Inc", alice, 150);
            Mycelium_String_print(techCorp.GetCompanyInfo() + "\n");

            // Test 4: Control Flow - For Loops
            Mycelium_String_print("\n--- Test 4: For Loops ---\n");
            for (int i = 0; i < 5; i = i + 1)
            {
                Mycelium_String_print("Loop iteration: " + i.ToString() + "\n");
                
                // Test nested object creation in loops
                if (i == 2)
                {
                    Person temp = new Person("Temp Person " + i.ToString(), 25, true, 5.5f);
                    Mycelium_String_print("Created temporary person: " + temp.name + "\n");
                    // temp goes out of scope here - should trigger destructor
                }
            }

            // Test 5: While Loops with Break/Continue
            Mycelium_String_print("\n--- Test 5: While Loops with Break/Continue ---\n");
            int counter = 0;
            while (counter < 10)
            {
                counter = counter + 1;
                
                if (counter == 3)
                {
                    Mycelium_String_print("Skipping counter: " + counter.ToString() + "\n");
                    continue;
                }
                
                if (counter == 7)
                {
                    Mycelium_String_print("Breaking at counter: " + counter.ToString() + "\n");
                    break;
                }
                
                Mycelium_String_print("Counter: " + counter.ToString() + "\n");
            }

            // Test 6: Calculator Operations
            Mycelium_String_print("\n--- Test 6: Calculator Operations ---\n");
            Calculator calc = new Calculator();
            
            int sum = calc.Add(15, 27);
            Mycelium_String_print("15 + 27 = " + sum.ToString() + "\n");
            
            int product = calc.Multiply(6, 8);
            Mycelium_String_print("6 * 8 = " + product.ToString() + "\n");
            
            Mycelium_String_print(calc.GetResultString() + "\n");

            // Test 7: Complex Object Interactions
            Mycelium_String_print("\n--- Test 7: Complex Object Array-like Usage ---\n");
            for (int j = 0; j < 3; j = j + 1)
            {
                Person employee = new Person("Employee " + j.ToString(), 25 + j, true, 5.5f + j.ToString().get_Length());
                Company dept = new Company("Department " + j.ToString(), employee, 10 + j * 5);
                
                Mycelium_String_print("Created: " + dept.GetCompanyInfo() + "\n");
                
                // Test conditional object creation
                if (j == 1)
                {
                    Calculator deptCalc = new Calculator();
                    int budget = deptCalc.Add(50000, j * 10000);
                    Mycelium_String_print("Department budget: " + budget.ToString() + "\n");
                }
            }

            // Test 8: String Operations
            Mycelium_String_print("\n--- Test 8: String Operations ---\n");
            string baseStr = "Sharpie Language";
            string substr = baseStr.Substring(8);
            Mycelium_String_print("Original: " + baseStr + "\n");
            Mycelium_String_print("Substring from 8: " + substr + "\n");
            Mycelium_String_print("Length: " + baseStr.get_Length().ToString() + "\n");

            // Test 9: Nested Loops with Objects
            Mycelium_String_print("\n--- Test 9: Nested Loops with Object Creation ---\n");
            for (int outer = 0; outer < 2; outer = outer + 1)
            {
                Mycelium_String_print("Outer loop: " + outer.ToString() + "\n");
                
                for (int inner = 0; inner < 3; inner = inner + 1)
                {
                    if (inner == 1)
                    {
                        continue; // Skip inner = 1
                    }
                    
                    Person nested = new Person("Nested_" + outer.ToString() + "_" + inner.ToString(), 20, true, 5.0f);
                    Mycelium_String_print("  Created: " + nested.name + "\n");
                    
                    if (inner == 2)
                    {
                        Calculator nestedCalc = new Calculator();
                        int value = nestedCalc.Add(outer * 10, inner * 5);
                        Mycelium_String_print("  Calculated: " + value.ToString() + "\n");
                    }
                }
            }

            // Test 10: Assignment and Reassignment (ARC Testing)
            Mycelium_String_print("\n--- Test 10: Object Assignment (ARC Testing) ---\n");
            Person original = new Person("Original Person", 35, true, 5.8f);
            Person copy = original; // Should increment ref count
            
            Mycelium_String_print("Original: " + original.name + "\n");
            Mycelium_String_print("Copy: " + copy.name + "\n");
            
            // Reassign to new object
            copy = new Person("New Person", 40, false, 6.1f);
            Mycelium_String_print("After reassignment - Copy: " + copy.name + "\n");

            // Test 11: Method Chaining and Complex Expressions
            Mycelium_String_print("\n--- Test 11: Complex Expressions ---\n");
            bool complexCondition = (testInt > 30) && (testBool == true);
            Mycelium_String_print("Complex condition result: " + complexCondition.ToString() + "\n");
            
            int mathResult = (testInt + 8) * 2 - 5;
            Mycelium_String_print("Math expression (42 + 8) * 2 - 5 = " + mathResult.ToString() + "\n");

            Mycelium_String_print("\n=== END OF COMPREHENSIVE TEST ===\n");
            Mycelium_String_print("All objects should now be destroyed as they go out of scope...\n\n");

            return 0;
        }
    }
}
