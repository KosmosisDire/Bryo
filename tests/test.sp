extern void Mycelium_String_print(string str);

class Program {
    static int Main()
    {
        Mycelium_String_print("Hello World");
        int x = 0;
        for (int i = 0; i < 10000; i = i + 1) {
            x = x + i;
            if (i % 100 == 0) {
                Mycelium_String_print("Current sum: " + x);
            }
        }
        Mycelium_String_print("Sum: " + x);
        Mycelium_String_print("Done");
        return 0;
    }
}