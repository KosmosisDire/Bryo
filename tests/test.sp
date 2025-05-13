
class Program
{
    static int Blah()
    {
        return 1239087;
    }

    static int Main()
    {
        int a = 1;
        int b = a + 17;

        if (b == 18)
        {
            return Program.Blah();
        }

        return 0;
    }
}