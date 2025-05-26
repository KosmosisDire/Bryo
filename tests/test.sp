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
            string combined = str + str2 + 5;

            
            
            Mycelium_String_print(combined);
            
            // Clean up strings
            Mycelium_String_delete(str);
            Mycelium_String_delete(str2);
            Mycelium_String_delete(combined);

            // --- Casting Tests ---

            // --- Integer to Integer ---
            int i_val1 = 10;
            long l_val1 = (long)i_val1;
            char c_val1 = (char)i_val1;

            long l_val2 = 123456789012L;
            int i_val2 = (int)l_val2;

            int i_val3 = -5;

            // --- Float to Float ---
            float f_val1 = 3.14f;
            double d_val1 = (double)f_val1;

            double d_val2 = 2.718281828459045;
            float f_val2 = (float)d_val2;

            // --- Integer to Float ---
            int i_val4 = 100;
            float f_val3 = (float)i_val4;
            double d_val3_cast = (double)i_val4; // Renamed to avoid conflict with d_val1, d_val2

            long l_val3 = -50L;
            float f_val4 = (float)l_val3;

            // --- Float to Integer ---
            float f_val5 = 123.75f;
            int i_val5 = (int)f_val5;

            double d_val4_cast = -456.25; // Renamed
            int i_val6 = (int)d_val4_cast;

            float f_val6 = 987.999f;
            long l_val4 = (long)f_val6;

            // --- Boolean to Integer ---
            bool b_true = true;
            bool b_false = false;
            int int_from_true = (int)b_true;
            int int_from_false = (int)b_false;

            // --- Integer to Boolean ---
            int int_zero = 0;
            int int_one = 1;
            int int_neg_one = -1;
            bool bool_from_zero = (bool)int_zero;
            bool bool_from_one = (bool)int_one;
            bool bool_from_neg_one = (bool)int_neg_one;

            // --- No-op Cast ---
            int i_val7 = 77;
            int i_val8 = (int)i_val7;

            // --- Char related (char is i8) ---
            char c_val_A = (char)65; // int to char
            
            // Use the variables to prevent them from being optimized away entirely
            // This is a simple way to ensure the cast operations are generated.
            // The return value will indicate if tests conceptually passed.
            double final_result = 0.0;
            if (l_val1 > 0 && c_val1 > 0 && i_val2 != 0 && i_val3 < 0 && // s_val1 and s_val2 conditions removed
                d_val1 > 0.0 && f_val2 > 0.0 && f_val3 > 0.0 && d_val3_cast > 0.0 && f_val4 < 0.0 &&
                i_val5 > 0 && i_val6 < 0 && l_val4 > 0 &&
                int_from_true == 1 && int_from_false == 0 &&
                bool_from_zero == false && bool_from_one == true && bool_from_neg_one == true &&
                i_val8 == 77 && c_val_A == 65) {
                final_result = 1.0; // All conditions met
            }
            
            return final_result + (double)i_val1; // Ensure some casted value is part of the return
        }
    }
}
