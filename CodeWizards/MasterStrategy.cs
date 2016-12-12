﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Remoting.Messaging;
using Com.CodeGame.CodeWizards2016.DevKit.CSharpCgdk.Model;

namespace Com.CodeGame.CodeWizards2016.DevKit.CSharpCgdk
{
    partial class MyStrategy
    {
        public Dictionary<long, AMessage> LastMessages = new Dictionary<long, AMessage>(); 

        public void MasterSendMessages()
        {
            if (World.TickIndex == 0)
            {
                var sorted = MyWizards.ToList();
                sorted.Sort((a, b) => Utility.Equals(a.X, b.X) ? a.Y.CompareTo(b.Y) : a.X.CompareTo(b.X));
                var order = new Dictionary<long, int>();
                for (var i = 0; i < sorted.Count; i++)
                    order[sorted[i].Id] = i;

                Func<AWizard, ALaneType> selectLane = x =>
                {
                    if (order[x.Id] <= 1)
                        return ALaneType.Top;
                    if (order[x.Id] == 4)
                        return ALaneType.Bottom;

                    return ALaneType.Middle;
                };

                Func<AWizard, AMessage> selectMessage = x =>
                {
                    return new AMessage(selectLane(x), SkillsGroup.Round2);
                };

                foreach (var x in MyWizards)
                    LastMessages[x.Id] = selectMessage(x);

                _redistributeSkills();
                _sendMessages();
            }
        }

        private void _sendMessages()
        {
            FinalMove.Messages = MyWizards
                    .Where(x => x.Id != Self.Id)
                    .OrderBy(x => x.Id)
                    .Select(x => LastMessages[x.Id].ToMessage())
                    .ToArray();

            MessagesObserver.LastMessage = LastMessages[ASelf.Id];
        }

        public void MasterCheckRearrange()
        {
            if (!Const.IsFinal)
                return;
            if (World.TickIndex > 2000)
                return;

            var allLanes = new[] {ALaneType.Middle, ALaneType.Top, ALaneType.Bottom};
            foreach (var lane in allLanes)
            {
                var oppsCount = SupportObserver.CountOpponentsOnLane(lane);
                var minesCount = LastMessages.Values.Count(x => x.Lane == lane);
                var target = RoadsHelper.GetLaneCenter(lane);
                if (oppsCount > minesCount + 1)
                {
                    long selSupportId = -1;
                    var maxDisbalance = int.MinValue;
                    foreach (var anotherLane in allLanes.Where(l => l != lane))
                    {
                        var oppsCount2 = SupportObserver.CountOpponentsOnLane(anotherLane);
                        var minesCount2 = LastMessages.Values.Count(x => x.Lane == anotherLane);
                        var disbalance = minesCount2 - oppsCount2;

                        if (disbalance >= maxDisbalance)
                        {
                            maxDisbalance = disbalance;
                            foreach(var w in MyWizards.Where(x => LastMessages[x.Id].Lane == anotherLane))
                                if (selSupportId == -1 || w.GetDistanceTo(target) < SupportObserver.LastSeen[selSupportId].Wizard.GetDistanceTo(target))
                                    selSupportId = w.Id;
                        }
                    }
                    if (selSupportId != -1)
                    {
                        LastMessages[selSupportId].Lane = lane;
                        _redistributeSkills();
                        _sendMessages();
                    }
                }
            }
        }

        private void _redistributeSkills()
        {
            var allLanes = new[] { ALaneType.Middle, ALaneType.Top, ALaneType.Bottom };
            foreach (var lane in allLanes)
            {
                var wizards = MyWizards.Where(w => LastMessages[w.Id].Lane == lane).ToArray();
                var skills = SkillGroups.GetDistribution(wizards.Length);
                foreach (var skill in skills)
                {
                    bool found = false;
                    for (var i = 0; i < wizards.Length; i++)
                    {
                        if (wizards[i] == null)
                            continue;
                        if (LastMessages[wizards[i].Id].SkillsGroup == skill)
                        {
                            found = true;
                            wizards[i] = null;
                            break;
                        }
                    }
                    if (!found)
                    {
                        for (var i = 0; i < wizards.Length; i++)
                        {
                            if (wizards[i] == null)
                                continue;
                            LastMessages[wizards[i].Id].SkillsGroup = skill;
                            wizards[i] = null;
                            break;
                        }
                    }
                }
            }
        }
    }
}
