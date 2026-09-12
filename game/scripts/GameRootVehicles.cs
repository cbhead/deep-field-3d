using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game;

public partial class GameRoot
{
    private readonly Dictionary<string, Vehicle> _vehicleNodes = new();

    /// <summary>Parks the map's vehicles. Their positions come from the map on
    /// the first frame and from whoever is driving after that.</summary>
    private void BuildVehicles(MapDef map)
    {
        foreach (var spawn in map.Vehicles)
        {
            var node = new Vehicle
            {
                Name = $"Vehicle_{spawn.Id}",
                Id = spawn.Id,
                Def = VehicleHandlings.For(spawn.DefId),
                Position = ToGd(spawn.Pos),
                Rotation = new Vector3(0, Mathf.DegToRad(spawn.YawDegrees), 0),
            };
            node.NetPosition = node.Position;
            node.NetYaw = node.Rotation.Y;
            AddChild(node);
            _vehicleNodes[spawn.Id] = node;
        }
        if (map.Vehicles.Count > 0)
            GD.Print($"[map] {map.Vehicles.Count} vehicle(s) parked");
    }

    public Vehicle? VehicleNode(string id) =>
        _vehicleNodes.TryGetValue(id, out var node) && IsInstanceValid(node) ? node : null;

    /// <summary>Hand each vehicle the sim's answer to the only question the sim
    /// owns — who is in it — and, for the ones this client is not driving,
    /// where the driver says it is.</summary>
    private void SyncVehicles()
    {
        foreach (var view in _view.Vehicles)
        {
            if (VehicleNode(view.Id) is not { } node) continue;
            bool mine = view.DriverId == LocalPlayerId;
            if (!mine && node.LocallyDriven)
            {
                // Just gave up the wheel: take the last word from the sim
                // rather than drifting away from it.
                node.NetPosition = view.Pos;
                node.NetYaw = Mathf.DegToRad(view.Yaw);
            }
            node.LocallyDriven = mine;
            if (mine) continue;
            node.NetPosition = view.Pos;
            node.NetYaw = Mathf.DegToRad(view.Yaw);
        }
    }

    /// <summary>The driver's word on where their vehicle is, on the same
    /// cadence as their own avatar and through the same channel. Nobody else
    /// sends one, and the sim drops it if they do.</summary>
    private void SendVehicleSync()
    {
        if (_view.SeatOf(LocalPlayerId) is not { } seated || seated.Seat != 0) return;
        if (VehicleNode(seated.Vehicle.Id) is not { } node) return;

        var pos = node.GlobalPosition;
        var command = new Command.VehicleSync(LocalPlayerId, node.Id,
            new Vec3(pos.X, pos.Y, pos.Z), Mathf.RadToDeg(node.Rotation.Y));
        if (Mode == RunMode.Client) _net.SendVehicle(node.Id, pos, node.Rotation.Y);
        else _world?.Enqueue(command);
    }

    /// <summary>Is this vehicle worth offering, and which seat? The driver's
    /// if it is free, else the first passenger seat — and the hint says which,
    /// because "[E] drive" on a full car is a promise the sim will break.</summary>
    public (string Id, int Seat, string Label)? VehicleOffer(string vehicleId)
    {
        if (_view.VehicleById(vehicleId) is not { } view) return null;
        string label = VehicleHandlings.For(view.DefId).Label;
        for (int seat = 0; seat < view.Seats.Length; seat++)
            if (view.SeatFree(seat)) return (view.Id, seat, label);
        return (view.Id, -1, label);
    }
}
