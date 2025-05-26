extern void Mycelium_String_print(string str);
extern void Mycelium_String_delete(string str);

namespace Test
{
    class Program
    {
        static double Main()
        {
            string str = "Hello World!";
            string str2 = " from Mycelium!\n";
            string combined = str + str2;

            
            
            Mycelium_String_print(combined);
            
            // Clean up strings
            Mycelium_String_delete(str);
            Mycelium_String_delete(str2);
            Mycelium_String_delete(combined);
            
            return 0.0;
        }
    }
}