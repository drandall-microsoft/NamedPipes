using System.Diagnostics;
using System.IO.Pipes;
using System.Runtime.CompilerServices;
using System.Text;

var pipe = new NamedPipeClientStream("ExamplePipeName");

Console.WriteLine("Connecting to server");
pipe.Connect();
pipe.ReadMode = PipeTransmissionMode.Message;
Console.WriteLine("Listener Connected!");
const int InitialBufferSize = 4096;
int numBytes = 0;
char[] buffer = new char[InitialBufferSize];

using var reader = new StreamReader(pipe);

using var writer = new StreamWriter(pipe);
writer.Write("listener");
writer.Flush();

StringBuilder sb = new StringBuilder();

int messageCount = 0;
var sw = Stopwatch.StartNew();
while (true)
{
    try
    {
        do
        {
            numBytes = reader.Read(buffer, 0, buffer.Length);
            sb.Append(buffer, 0, numBytes);
        } while (!pipe.IsMessageComplete);
        messageCount++;
        if(messageCount % 404 == 0)
        {
            Console.WriteLine("Elapsed ms: " + sw.ElapsedMilliseconds);
        } else if(messageCount % 404 == 1)
        {
            sw.Restart();
        }
        sb.Clear();
    } catch(Exception)
    {
        if(!pipe.IsConnected)
        {
            Console.WriteLine("Reconnecting");
            pipe.Connect();
        }
    }
}
