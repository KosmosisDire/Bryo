namespace My.App;

public class Processor
{
    int value;
    int anotherValue;
    bool flag;

    // Method to test statement parsing inside its body
    public void ProcessValues()
    {
        // Assignment Expression Statements
        value = 10;
        anotherValue = value; 
        value += 5;
        anotherValue -= 2;

        // Unary Expression Statements
        ++value;
        --anotherValue;
        // flag = !flag; // This would be `flag = UnaryOp(!, Identifier(flag))`

        // Local Variable Declarations (from M3, now with initializers using M5 expressions)
        int localVar = 100;
        int simpleInit = value;
        bool invertedFlag = !flag; // `!flag` is UnaryOp(!, Identifier(flag))

        // Return Statements
        return; 
    }

    // Method returning a value
    public int GetCalculatedValue()
    {
        int intermediate = value;
        return ++intermediate; // Returns the result of pre-increment
    }

    public bool GetFlag()
    {
        return flag;
    }

    public bool GetInvertedFlag()
    {
        bool currentFlag = flag;
        return !currentFlag;
    }
    
    public void EmptyReturnMethod()
    {
        return;
    }
}

public class InitializersTest
{
    // Field initializers using M5 expressions
    int x = 100;
    int y = -50; // Unary minus on a literal
    int z = x;   // Identifier
    bool active = true;
    bool inactive = !active; // Unary not on an identifier
    // string text = "hello"; // String literal (M3)

    // This would still require 'new' to be more fully integrated beyond a primary expression stub for ObjectCreation.
    // MyType complex = new MyType;
}