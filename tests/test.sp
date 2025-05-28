extern void Mycelium_String_print(string str);
extern void Mycelium_String_delete(string str);
extern void print_int(int val);
extern void print_double(double val);
extern void print_bool(bool val);

namespace TestSuite
{
    class Console
    {
        static void Log(string message)
        {
            Mycelium_String_print(message + "\n");
        }

        static void LogInt(int value)
        {
            print_int(value);
        }

        static void LogDouble(double value)
        {
            print_double(value);
        }

        static void LogBool(bool value)
        {
            print_bool(value);
        }
    }

    class Point
    {
        int x;
        int y;

        // Constructor
        Point(int initialX, int initialY)
        {
            this.x = initialX;
            this.y = initialY;
            Console.Log("Point ctor called: (" + this.x + ", " + this.y + ")");
        }

        // Destructor
        ~Point()
        {
            Console.Log("Point dtor called: (" + this.x + ", " + this.y + ")");
            // Implicitly, this.x and this.y are not strings, so no need to delete them
            // If they were strings, we would call Mycelium_String_delete on them
        }

        void SetCoordinates(int newX, int newY)
        {
            this.x = newX;
            this.y = newY;
        }

        int GetX()
        {
            return this.x;
        }

        int GetY()
        {
            return this.y;
        }

        void Print()
        {
            Mycelium_String_print("Point: (");
            print_int(this.x);
            Mycelium_String_print(", ");
            print_int(this.y);
            Mycelium_String_print(")\n");
        }

        static Point CreateOrigin()
        {
            Mycelium_String_print("CreateOrigin called\n");
            Point origin = new Point(0, 0);
            return origin;
        }
    }

    class TestRunner
    {
        static void TestObjectCreationAndMethods()
        {
            Mycelium_String_print("--- TestObjectCreationAndMethods ---\n");
            Point p1 = new Point(10, 20);
            p1.Print(); // Expected: Point: (10, 20)

            p1.SetCoordinates(15, 25);
            p1.Print(); // Expected: Point: (15, 25)

            int currentX = p1.GetX();
            int currentY = p1.GetY();
            Mycelium_String_print("GetX returned: "); print_int(currentX); Mycelium_String_print("\n"); // Expected: 15
            Mycelium_String_print("GetY returned: "); print_int(currentY); Mycelium_String_print("\n"); // Expected: 25

            Point p2 = Point.CreateOrigin();
            p2.Print(); // Expected: Point: (0, 0)

            // Test assignment and ARC (p1's original object should be released if ARC is working)
            p1 = p2;
            Mycelium_String_print("p1 after assignment (p1=p2):\n");
            p1.Print(); // Expected: Point: (0,0)
            p2.SetCoordinates(5, 5);
            Mycelium_String_print("p1 after p2 modified (should also be 5,5):\n");
            p1.Print(); // Expected: Point: (5,5)

            // Mycelium_String_delete for p1 and p2 happens implicitly at scope end if ARC handles it.
            // If manual deletion is required by your string type, it should be done.
            // For Point objects, ARC should handle them.
        }

        static void TestControlFlowAndPrimitives()
        {
            Mycelium_String_print("--- TestControlFlowAndPrimitives ---\n");
            int a = 10;
            int b = 20;
            if (a < b)
            {
                Mycelium_String_print("a < b is true\n"); // Expected
            }
            else
            {
                Mycelium_String_print("a < b is false (ERROR)\n");
            }
        }

        static void TestStringOperations()
        {
            Mycelium_String_print("--- TestStringOperations ---\n");
            string greeting = "Hello";
            string name = "Mycelium";
            string message = greeting + " " + name + "!";
            Mycelium_String_print(message); Mycelium_String_print("\n"); // Expected: Hello Mycelium!
            Mycelium_String_delete(greeting);
            Mycelium_String_delete(name);
            Mycelium_String_delete(message);

            string num_str = "Value: " + 42;
            Mycelium_String_print(num_str); Mycelium_String_print("\n"); // Expected: Value: 42
            Mycelium_String_delete(num_str);
        }
    }

    class ResourceHog
    {
        string name;
        int id;

        ResourceHog(string n, int i)
        {
            this.name = n;
            this.id = i;
            Console.Log("ResourceHog '" + this.name + "' (" + "ID: ");
            Console.LogInt(this.id);
            Console.Log(") created.");
        }

        ~ResourceHog()
        {
            Console.Log("ResourceHog '" + this.name + "' (" + "ID: ");
            Console.LogInt(this.id);
            Console.Log(") destroyed.");
            // Implicitly, this.name (a string) should be released by compiler-generated dtor code
        }

        void Greet()
        {
            Console.Log("Greetings from " + this.name);
        }
    }

    class Program
    {
        static void TestScope()
        {
            Console.Log("Entering TestScope...");
            ResourceHog hog1 = new ResourceHog("HogInScope", 1);
            hog1.Greet();
            Console.Log("Leaving TestScope...");
            // hog1 should be destroyed here
        }

        static int Main()
        {
            Console.Log("--- Program Start ---");

            Program.TestScope(); // Test destructor on scope exit

            Console.Log("--- Reassignment Test ---");
            ResourceHog hog2 = new ResourceHog("FirstHog", 2);
            hog2 = new ResourceHog("SecondHog", 3); // "FirstHog" should be destroyed here
            Console.Log("Hog2 is now SecondHog.");
            // "SecondHog" will be destroyed at the end of Main's scope

            TestRunner.TestObjectCreationAndMethods();
            TestRunner.TestControlFlowAndPrimitives();
            TestRunner.TestStringOperations();

            Console.Log("--- Program End ---");
            return 0;
        }
    }
}