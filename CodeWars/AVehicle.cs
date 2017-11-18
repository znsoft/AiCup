﻿using System;
using System.Linq;
using Com.CodeGame.CodeWars2017.DevKit.CSharpCgdk.Model;

namespace Com.CodeGame.CodeWars2017.DevKit.CSharpCgdk
{
    public class AVehicle : ACircularUnit
    {
        public bool IsMy;
        public VehicleType Type;
        public int Durability;
        public bool IsSelected;
        public double MaxSpeed; // TODO: computable property
        public double VisionRange; // TODO: computable property
        public int RemainingAttackCooldownTicks;
        public uint Groups;

        public Point MoveTarget;
        public double MoveSpeed;
        public Point RotationCenter;
        public double RotationAngle;
        public double RotationAngularSpeed;
        public int DurabilityPool;

        public AVehicle(Vehicle unit) : base(unit)
        {
            IsMy = unit.PlayerId == MyStrategy.Me.Id;
            Type = unit.Type;
            Durability = unit.Durability;
            IsSelected = unit.IsSelected;
            MaxSpeed = unit.MaxSpeed;
            RemainingAttackCooldownTicks = unit.RemainingAttackCooldownTicks;
            VisionRange = unit.VisionRange;
            foreach (var group in unit.Groups)
                AddGroup(group);
        }

        public AVehicle(AVehicle unit) : base(unit)
        {
            IsMy = unit.IsMy;
            Type = unit.Type;
            Durability = unit.Durability;
            IsSelected = unit.IsSelected;
            MaxSpeed = unit.MaxSpeed;
            RemainingAttackCooldownTicks = unit.RemainingAttackCooldownTicks;
            VisionRange = unit.VisionRange;
            Groups = unit.Groups;

            MoveTarget = unit.MoveTarget;
            MoveSpeed = unit.MoveSpeed;
            RotationCenter = unit.RotationCenter;
            RotationAngle = unit.RotationAngle;
            RotationAngularSpeed = unit.RotationAngularSpeed;
            DurabilityPool = unit.DurabilityPool;
        }

        public double FullDurability => Durability + (double) DurabilityPool/G.ArrvRepairPoints;

        public void Repair()
        {
            if (Durability == G.MaxDurability)
                return;
            DurabilityPool++;
            if (DurabilityPool == G.ArrvRepairPoints)
            {
                Durability++;
                DurabilityPool = 0;
            }
        }

        public bool HasGroup(int groupId)
        {
            return (Groups & (1U << groupId)) != 0;
        }

        public void AddGroup(int groupId)
        {
            Groups |= 1U << groupId;
        }

        public void RemoveGroup(int groupId)
        {
            Groups &= (1U << groupId) ^ 0xFFFFFFFF;
        }

        public bool IsAerial => Type == VehicleType.Helicopter || Type == VehicleType.Fighter;

        public bool Move(Func<AVehicle, bool> checkCollisions = null)
        {
            var newRotationAngle = RotationAngle;

            Point delta = null;
            var done = false;

            if (MoveTarget != null)
            {
                var vec = MoveTarget - this;
                double length;
                var speed = ActualSpeed;

                if (vec.Length <= speed)
                {
                    done = true;
                    length = vec.Length;
                }
                else
                {
                    length = speed;
                }
                delta = vec.Normalized()*length;
            }
            else if (RotationCenter != null)
            {
                var angle = ActualAngularSpeed;
                if (angle >= Math.Abs(RotationAngle))
                {
                    done = true;
                    angle = RotationAngle;
                }
                else
                {
                    if (RotationAngle < 0)
                        angle = -angle;

                    newRotationAngle -= angle;
                }
                var to = RotateCounterClockwise(angle, RotationCenter);
                delta = to - this;
            }

            if (delta != null)
            {
                var selfCopy = new AVehicle(this);
                selfCopy.X += delta.X;
                selfCopy.Y += delta.Y;
                selfCopy.RotationAngle = newRotationAngle;

                if (selfCopy.X < Radius - Const.Eps ||
                    selfCopy.Y < Radius - Const.Eps ||
                    selfCopy.X > G.MapSize - Radius + Const.Eps ||
                    selfCopy.Y > G.MapSize - Radius + Const.Eps ||
                    checkCollisions != null && checkCollisions(selfCopy))
                {
                    return false;
                }

                X += delta.X;
                Y += delta.Y;
                RotationAngle = newRotationAngle;

                if (done)
                {
                    MoveTarget = null;
                    MoveSpeed = 0;
                    RotationCenter = null;
                    RotationAngle = 0;
                    RotationAngularSpeed = 0;
                }
            }

            Utility.Dec(ref RemainingAttackCooldownTicks);

            return true;
        }

        public double ActualSpeed
        {
            get
            {
                var speed = MaxSpeed;
                if (IsAerial)
                {
                    var weather = MyStrategy.Weather(X, Y);
                    if (weather == WeatherType.Cloud)
                        speed *= G.CloudWeatherSpeedFactor;
                    else if (weather == WeatherType.Rain)
                        speed *= G.RainWeatherSpeedFactor;
                }
                else
                {
                    var terrian = MyStrategy.Terrain(X, Y);
                    if (terrian == TerrainType.Swamp)
                        speed *= G.SwampTerrainSpeedFactor;
                    else if (terrian == TerrainType.Forest)
                        speed *= G.ForestTerrainSpeedFactor;
                }
                if (MoveSpeed > 0 && MoveSpeed < speed)
                    speed = MoveSpeed;
                return speed;
            }
        }

        public double ActualAngularSpeed
        {
            get
            {
                var speed = ActualSpeed;
                var angle = speed/GetDistanceTo(RotationCenter);
                if (RotationAngularSpeed > 0 && RotationAngularSpeed < angle)
                    angle = RotationAngularSpeed;
                return angle;
            }
        }

        public bool IsAlive => Durability > Const.Eps;

        public int GetAttackDamage(AVehicle veh, double additionalRadius = 0)
        {
            var attackRange = Geom.Sqr(G.AttackRange[(int) Type, (int) veh.Type] + additionalRadius);

            if (GetDistanceTo2(veh) - Const.Eps > attackRange)
                return 0;
            var damage = G.AttackDamage[(int) Type, (int) veh.Type];
            if (damage >= veh.Durability)
                return veh.Durability;
            return damage;
        }

        public void Attack(AVehicle veh)
        {
            if (RemainingAttackCooldownTicks > 0)
                return;

            var damage = GetAttackDamage(veh);
            veh.Durability -= damage;
            RemainingAttackCooldownTicks = G.AttackCooldownTicks;
        }

        public int GetNuclearDamage(ANuclear nuclear)
        {
            var dist2 = GetDistanceTo2(nuclear);
            if (dist2 >= G.TacticalNuclearStrikeRadius * G.TacticalNuclearStrikeRadius)
                return 0;
            var damage = (int)((G.TacticalNuclearStrikeRadius - Math.Sqrt(dist2)) * G.MaxTacticalNuclearStrikeDamage);
            return Math.Min(damage, Durability);
        }

        public void ForgotTarget()
        {
            MoveTarget = null;
            MoveSpeed = 0;
            RotationCenter = null;
            RotationAngle = 0;
            RotationAngularSpeed = 0;
        }

        public bool IsGroup(MyGroup group)
        {
            return group.Type == Type ||
                   group.Group != null && HasGroup((int) group.Group);
        }

        public double ActualVisionRange
        {
            get
            {
                var res = VisionRange;
                if (IsAerial)
                {
                    var weather = MyStrategy.Weather(X, Y);
                    if (weather == WeatherType.Cloud)
                        res *= G.CloudWeatherVisionFactor;
                    else if (weather == WeatherType.Rain)
                        res *= G.RainWeatherVisionFactor;
                }
                else
                {
                    var terrian = MyStrategy.Terrain(X, Y);
                    if (terrian == TerrainType.Forest)
                        res *= G.ForestTerrainVisionFactor;
                }
                return res;
            }
        }
    }
}
