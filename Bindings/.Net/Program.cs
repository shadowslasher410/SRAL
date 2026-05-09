class Program
{
    static void Main(string[] args)
    {
        try {
            RunMainDemo();
        }
        catch (Exception e) {
            Console.WriteLine($"Demo failed with error: {e.Message}");
        }

        ErrorHandlingDemo();
    }

    static void RunMainDemo()
    {
        using var sral = new SRAL();
        Console.WriteLine("=== SRAL Library Demo ===");

        if (!sral.IsInitialized()) {
            Console.WriteLine("Initializing SRAL...");
            if (!sral.Initialize()) {
                Console.WriteLine("Failed to initialize SRAL!");
                return;
            }
        }

        Console.WriteLine("SRAL initialized successfully!");
        var current = sral.GetCurrentEngine();
        Console.WriteLine($"Current Engine: {sral.GetEngineName(current)}");

        Console.WriteLine("Speaking basic text...");
        sral.Speak("Hello, this is a test of the SRAL library.", true);

        sral.Delay(1000);
        Console.WriteLine("Cleaning up...");
    }

    static void ErrorHandlingDemo()
    {
        Console.WriteLine("\n=== Error Handling Demo ===");
        try {
            var sral = new SRAL();
            Console.WriteLine("Attempting operation without initialization...");
            bool result = sral.Speak("This might fail depending on library state", true);
            Console.WriteLine($"Result: {result}");
        }
        catch (Exception e) {
            Console.WriteLine($"Error caught: {e.Message}");
        }
    }
}