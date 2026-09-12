using Godot;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;

namespace DeepField.Game;

/// <summary>The runbook's "curl before you invite anyone" endpoint, ported from
/// the 2D relay: GET /info → {tailscaleIp, port, version, players, wave}.
/// Deliberately no CORS — the response names the host's tailnet IP.</summary>
public partial class InfoServer : Node
{
    private TcpServer _server = new();
    public System.Func<Godot.Collections.Dictionary>? StatusProvider;

    public string? TailscaleIp { get; private set; }
    private int _port;

    /// <summary>Whether the bind actually took. False means this server answers
    /// nothing, which callers must decide what to do about.</summary>
    public bool Listening { get; private set; }

    /// <summary>Binds the info port, returning the bind result rather than
    /// swallowing it.
    ///
    /// The return used to be discarded, and the success line printed either
    /// way. That is a bad failure to hide: the info port is TCP while the game
    /// is ENet/UDP, so losing this bind costs the server nothing it can feel —
    /// it hosts and plays perfectly while answering nothing. The runbook's
    /// "curl before you invite anyone" step then reports whatever *did* win the
    /// port, and both 2D checkouts default to the same one, so the honest
    /// answer is not "no reply" but a confident description of a different
    /// game.</summary>
    public Error Start(int port)
    {
        _port = port;
        TailscaleIp = DetectTailscaleIp();

        var err = _server.Listen((ushort)port);
        if (err != Error.Ok)
        {
            Listening = false;
            return err;
        }

        Listening = true;
        GD.Print($"[info] http://{TailscaleIp ?? "localhost"}:{port}/info");
        return Error.Ok;
    }

    public override void _Process(double delta)
    {
        if (!Listening) return;

        while (_server.IsConnectionAvailable())
        {
            var conn = _server.TakeConnection();
            // Read whatever arrived; we only serve one route, so no parsing rigor.
            conn.GetData((int)conn.GetAvailableBytes());

            var status = StatusProvider?.Invoke() ?? new Godot.Collections.Dictionary();
            status["tailscaleIp"] = TailscaleIp ?? "";
            status["port"] = _port;
            status["version"] = DeepField.Sim.Protocol.Version;

            string body = Json.Stringify(status);
            string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n" +
                              $"Content-Length: {Encoding.UTF8.GetByteCount(body)}\r\n" +
                              "Connection: close\r\n\r\n" + body;
            conn.PutData(Encoding.UTF8.GetBytes(response));
            conn.DisconnectFromHost();
        }
    }

    /// <summary>Tailscale IPv4 lives in the CGNAT range 100.64.0.0/10 — the same
    /// detection the 2D server shipped with.</summary>
    private static string? DetectTailscaleIp()
    {
        try
        {
            foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (nic.OperationalStatus != OperationalStatus.Up) continue;
                foreach (var addr in nic.GetIPProperties().UnicastAddresses)
                {
                    if (addr.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                    var bytes = addr.Address.GetAddressBytes();
                    if (bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127)
                        return addr.Address.ToString();
                }
            }
        }
        catch (NetworkInformationException) { }
        return null;
    }
}
