using System.IO.Pipes;

var pipe = new NamedPipeClientStream("SqTechPipe");
Console.WriteLine("Connecting to server");
pipe.Connect();
Console.WriteLine("Commander Connected!");

using var writer = new StreamWriter(pipe);

while (true)
{
    try
    {
        Console.WriteLine("Enter a command");
        var command = Console.ReadLine();
        if (string.IsNullOrEmpty(command))
        {
            break;
        }
        writer.Write(command);
        writer.Flush();
    }
    catch(Exception)
    {
        if(!pipe.IsConnected)
        {
            Console.WriteLine("Reconnecting");
            pipe.Connect();
        }
    }
}