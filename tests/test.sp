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
            Console.Log("Create Point: (" + this.x + ", " + this.y + ")");
        }

        // Destructor
        ~Point()
        {
            Console.Log("Destroy Point: (" + this.x + ", " + this.y + ")");
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
        }

        static int Main()
        {
            Console.Log("--- Program Start ---");

            Program.TestScope();

            Point p = new Point(42, 0);
            Point p2 = Point.CreateOrigin();
            p2.Print();
            Console.Log("Point p2 X: " + p2.GetX());
            Console.Log("Point p2 Y: " + p2.GetY());

            Console.Log("--- Program End ---");
            return 0;
        }
    }
}