extern void Mycelium_String_print(string str);

class Console
{
    public static void Log(string message)
    {
        Mycelium_String_print(message);
    }
}

class Program
{
    static int Main()
    {
        Console.Log("Hello, World!");
        string name = "Mycelium";
        Console.Log("Hello, " + name + "!");
        int number = 42;
        return 0;
    }
}