namespace My.LoopDemo;

public class DataProcessor
{
    // M10: Array type in field declaration
    public int[] dataValues;
    public string[][] categories; // Jagged array type
    public int x, y, z;
    public List<string> listTest;

    public DataProcessor(int size)
    {
        // Array creation `new int[size]` is more advanced (not explicitly M10 parsing, but type is)
        // For now, assume dataValues is initialized elsewhere or null.
        // this.dataValues = new int[size]; 
        this.dataValues = null; // M3: null literal
    }

    public int SumValues()
    {
        int sum = 0;
        // M10: ForEach loop
        // For M10, dataValues will be an IdentifierExpression.
        // If dataValues is null, this would be a runtime error, not a parse error.
        if (this.dataValues != null) // M6: check for null
        {
            foreach (int value in this.dataValues) 
            {
                sum += value; // M5: compound assignment
            }
        }
        return sum;
    }

    public void ProcessCategories(int limit)
    {
        // M10: For loop
        for (int i = 0; i < limit && i < 10; i = i + 1) // M3: local var decl; M6: conditions; M5: assignment
        {
            if (i == 5) // M6: equality
            {
                // M10: Continue statement
                continue; 
            }

            // string currentCategory = this.categories[i][0]; // Array element access is future
            string placeholder = "Processing category index "; // M3
            placeholder = placeholder + i; // M6

            if (i == 8)
            {
                // M10: Break statement
                break;
            }
        }
    }
    
    public string[] GetSampleStrings() 
    {
        // M10: Array type as return type
        return null; // M5: return null
    }

    public List<List<string>> GetSampleLists() 
    {
        // M10: List type as return type
        return null; // M5: return null
    }
}
