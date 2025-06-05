extern void Mycelium_String_print(string str);
extern void print_int(int val);

namespace TestPrimitiveStructs
{
    class Program
    {
        static int Main()
        {
            Mycelium_String_print("=== Testing Primitive Struct Methods ===\n");

            // Test int.ToString()
            int number = 42;
            string numberStr = number.ToString();
            Mycelium_String_print("number.ToString(): ");
            Mycelium_String_print(numberStr);
            Mycelium_String_print("\n");

            // Test int.Parse() static method
            string parseTest = "123";
            int parsed = int.Parse(parseTest);
            Mycelium_String_print("int.Parse(\"123\"): ");
            print_int(parsed);
            Mycelium_String_print("\n");

            // Test bool.ToString()
            bool flag = true;
            string flagStr = flag.ToString();
            Mycelium_String_print("flag.ToString(): ");
            Mycelium_String_print(flagStr);
            Mycelium_String_print("\n");

            // Test string.Length property
            string testStr = "Hello World";
            int length = testStr.get_Length();
            Mycelium_String_print("\"Hello World\".Length: ");
            print_int(length);
            Mycelium_String_print("\n");

            // Test string.Substring()
            string substr = testStr.Substring(6);
            Mycelium_String_print("\"Hello World\".Substring(6): ");
            Mycelium_String_print(substr);
            Mycelium_String_print("\n");

            // Test string.Empty static property
            string empty = string.get_Empty();
            Mycelium_String_print("string.Empty length: ");
            print_int(empty.get_Length());
            Mycelium_String_print("\n");

            Mycelium_String_print("=== All primitive struct tests completed ===\n");
            return 0;
        }
    }
}
