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

    class Program 
    {
        static int Main() 
        {
            Console.Log("Hello World!");

            var str = "Hello Mycelium!";
            Console.Log(str);
            return 0;
        }
    }
}
