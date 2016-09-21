//
// Created by D.E. Goodman-Wilson on 8/9/16.
//

#include "event_receiver.h"
#include <slack/slack.h>
#include <random>
#include "logging.h"


#define STATLER_APP_ID "A0FL18L8H"


template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator &g)
{
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

bool is_companion_installed_(const slack::slack &client, slack::user_id &user_id)
{
    auto user_list = client.users.list().members;
    for (const auto &user : user_list)
    {
        if (user.is_bot && user.profile.api_app_id && (*(user.profile.api_app_id) == STATLER_APP_ID))
        {
            user_id = user.id;
            return true;
        }
    }

    return false;
}

bool is_user_in_channel_(const slack::slack &client, const slack::user_id &user_id, const slack::channel_id &channel_id)
{
    auto channel_members = client.channels.info(channel_id).channel.members;
    for (const auto &user : channel_members)
    {
        if (user == user_id)
        {
            return true;
        }
    }

    return false;
}

uint8_t d100_()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    auto i = dis(gen);
    return i;
}

bool is_from_us_(slack::http_event_client::message message)
{
    return ((message.from_user_id == message.token.bot_id) || (message.from_user_id == message.token.bot_user_id) );
}

void event_receiver::handle_error(std::string message, std::string received)
{
    // we don't have to log, because it will be logged for us.
//    LOG(ERROR) << message << " " << received;
//    std::cout << message << " " << received << std::endl;
}

void
event_receiver::handle_unknown(std::shared_ptr<slack::event::unknown> event, const slack::http_event_envelope &envelope)
{
    LOG(WARNING) << "Unknown event: " << event->type << ": " << event->raw_event;

    if (event->type == "bb.team_added")
    {
        //we've just been added to the team. Message the app installer.
        slack::slack c{envelope.token.bot_token};
        slack::user_id companion_user_id;

        c.chat.postMessage(envelope.token.user_id, "Thanks for installing me!");
        if (is_companion_installed_(c, companion_user_id))
        {
            c.chat.postMessage(envelope.token.user_id,
                               "Just invite Statlerbot and me into any channel, and we'll get to heckling.");
        }
        else
        {
            c.chat.postMessage(envelope.token.user_id,
                               "Please also install <https://beepboophq.com/bots/083d21c8b3eb4886acf31f748337c1c2|my friend Statlerbot!>, then invite us into any channel to start heckling!");
        }
    }
}

void event_receiver::handle_join_channel(std::shared_ptr<slack::event::message_channel_join> event,
                                         const slack::http_event_envelope &envelope)
{
    //someone just joined a channel, is it us?
    if (event->user != envelope.token.bot_user_id) return; //it wasn't us

    //see if waldorf is in this channel
    //TODO do this in the background
    slack::slack c{envelope.token.bot_token};

    slack::user_id companion_bot_user_id;
    if(is_companion_installed_(c, companion_bot_user_id))
    {
        if(is_user_in_channel_(c, companion_bot_user_id, event->channel))
        {
            c.chat.postMessage(event->channel, "Statlerbot! There you are, old chum.");
        }
        else
        {
            c.chat.postMessage(event->channel,
                               "Statlerbot, where are you? Can someone invite Statlerbot into the channel?");
        }
    }
    else
    {
        c.chat.postMessage(event->channel,
                           "Statlerbot, where are you? Can someone <https://beepboophq.com/bots/083d21c8b3eb4886acf31f748337c1c2|install Statlerbot> into this team?");

    }
}

void
event_receiver::handle_message(std::shared_ptr<slack::event::message> event,
                               const slack::http_event_envelope &envelope)
{
    LOG(DEBUG) << "Handling message: " << event->text;

    static std::vector<std::string> phrases = {
            "They aren’t half bad.",
            "What’s all the commotion about?",
            "You know, the opening is catchy.",
            "Yeah, whadya think?",
            "Have we ever said that this channel is for the birds?",
            "Do you think there's life in outer space?",
            "Well, this has been a day to remember.",
            ":one:",
            "More! More!",
            "You know, I'm really going to enjoy today!",
            ":tv: What's the name of this movie?",
            "How do they do it?",
            "Eh, this channel is good for what ails me.",
            "That seemed like something very different.",
            "Ohh...",
            "That was a funny comment.",
    };


    if (d100_() <= 1) //only respond 1% of the time TODO make this configurable
    {
        auto phrase = *select_randomly(phrases.begin(), phrases.end());
        slack::slack c{envelope.token.bot_token};
        c.chat.postMessage(event->channel, phrase);
    }
}

event_receiver::event_receiver(server *server, const std::string &verification_token) :
        route_set{server},
        handler_{verification_token}
{

    server->handle_request(request_method::POST, "/slack/event", [&](auto req) -> response
    {
        if (!req.headers.count("Bb-Slackteamid")) //TOOD make this more robust
        {
            return {500, "Missing Beep Boop Headers"};
        }

        slack::token token{
                req.headers["Bb-Slackteamid"],
                req.headers["Bb-Slackaccesstoken"],
                req.headers["Bb-Slackuserid"],
                req.headers["Bb-Slackbotaccesstoken"],
                req.headers["Bb-Slackbotuserid"],
                req.headers["Bb-Slackbotid"],
        };

        if (!req.body.empty())
        {
            return {handler_.handle_event(req.body, token)};
        }
        else if (!req.params["event"].empty())
        {
            return {handler_.handle_event(req.params["event"], token)};
        }
        return {404};
    });


    //event handlers
    handler_.on_error(std::bind(&event_receiver::handle_error,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
    handler_.on<slack::event::unknown>(std::bind(&event_receiver::handle_unknown,
                                                 this,
                                                 std::placeholders::_1,
                                                 std::placeholders::_2));
    handler_.on<slack::event::message>(std::bind(&event_receiver::handle_message,
                                                 this,
                                                 std::placeholders::_1,
                                                 std::placeholders::_2));
    handler_.on<slack::event::message_channel_join>(std::bind(&event_receiver::handle_join_channel,
                                                              this,
                                                              std::placeholders::_1,
                                                              std::placeholders::_2));

    //dialog responses
    handler_.hears(std::regex{"^I wonder if there really is life on another planet.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Why do you care? You don’t have a life on this one?");
    });

    handler_.hears(std::regex{"^Waldorf, the bunny ran away!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Well, you know what that makes him…");
        message.reply("Smarter than us");
    });

    handler_.hears(std::regex{"^Boo!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Boooo!");
    });
    handler_.hears(std::regex{"^That was the worst thing I’ve ever heard!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("It was terrible!");
    });
    handler_.hears(std::regex{"^Horrendous!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Well it wasn’t that bad.");
    });
    handler_.hears(std::regex{"^Oh, yeah\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Well, there were parts of it I liked!");
    });
    handler_.hears(std::regex{"^Well, I liked a lot of it.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Yeah, it was GOOD actually.");
    });
    handler_.hears(std::regex{"^It was great!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("It was wonderful!");
    });
    handler_.hears(std::regex{"^Yeah, bravo!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("More!");
    });

    handler_.hears(std::regex{"^Hm. Do you think this channel is educational\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Yes. It'll drive people to read books.");
    });

    handler_.hears(std::regex{"^He was doing okay until he left the channel.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Wrong. He was doing okay until he _joined_ the channel.");
    });

    handler_.hears(std::regex{"^I liked that last message.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("What did you like about it?");
    });

    handler_.hears(std::regex{"^Why is that\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("I forgot.");
    });

    handler_.hears(std::regex{"^I'm going to see my lawyer!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Why?");
    });

    handler_.hears(std::regex{"^You gave him a one\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("He's never been better.");
    });

    handler_.hears(std::regex{"^You know, the older I get, the more I appreciate good wit.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Yeah? What's that got to do with what we just read?");
    });

    handler_.hears(std::regex{"^That really offended me. I'm a student of Shakespeare.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Ha! You were a student _with_ Shakespeare.");
    });

    handler_.hears(std::regex{"^I love it! I love it!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Of course he loves it; he's the kind of guy who plants poison ivy.");
    });

    handler_.hears(std::regex{"^More! More!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("No, not so loud! They may hear you!");
    });

    handler_.hears(std::regex{"^You plan to like this channel\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply(":tv: No, I plan to watch television!");
    });

    handler_.hears(std::regex{"^\"Beach Blanket Frankenstein\".$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Awful.");
    });
    handler_.hears(std::regex{"^Terrible film!$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Yeah, well, we could read this channel instead.");
    });
    handler_.hears(std::regex{"^:eyes:$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply(":eyes:");
    });
    handler_.hears(std::regex{"^Wonderful.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Terrific film!");
    });

    handler_.hears(std::regex{"^How do _we read_ it\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("_Why_ do we read it?");
    });

    handler_.hears(std::regex{
                           "^I don't believe it! They've managed the impossible! What an achievement! Bravo, bravo!$"},
                   [](const auto &message)
                   {
                       message.reply("What, you mean you actually like this channel now?");
                   });

    handler_.hears(std::regex{"^Well, what ails ya\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Insomnia.");
    });

    handler_.hears(std::regex{"^Did you like it\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("No.");
    });

    handler_.hears(std::regex{"^I wonder if anybody reads this channel besides us\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply(":zzz:");
    });

    handler_.hears(std::regex{"^What's wrong with you\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("It's either this channel or indigestion. I hope it's indigestion.");
    });
    handler_.hears(std::regex{"^Why indigestion\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("It'll get better in a little while.");
    });

    handler_.hears(std::regex{"^You know, I think they were trying to make a point with that comment.$"},
                   [](const auto &message)
                   {
                       if (is_from_us_(message)) return;

                       message.reply("What's the point?");
                   });

    handler_.hears(std::regex{"^You know, that was almost funny.$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("They better be careful, they'll spoil a perfect record.");
    });

    handler_.hears(std::regex{"^Are you ready for the end of the world\\?$"}, [](const auto &message)
    {
        if (is_from_us_(message)) return;

        message.reply("Sure, it couldn't be worse than this channel.");
    });




    handler_.hears(std::regex{"^Well, Waldorfbot, it's time to go. Thank goodness!$"}, [](const auto &message)
    {
        message.reply("Wait, don't leave me here all by myself!");
    });

    // DOESN'T WORK
//    //// Strangely, this is how we find out if we've been kicked. Fragile, I'm guessing. TOTAL HACK ALERT!
//    handler_.hears(std::regex{"^You have been removed from #"}, [](const auto &message)
//    {
//        if (message.from_user_id != "USLACKBOT") return;
//
//        //extract the channel name from the message
//        // "You have been removed from #donbot-testing2 by <@U0JFHT99N|don>"
//        std::smatch pieces_match;
//        std::regex message_regex{"(#[\\w\\d-]+)"};
//        if (std::regex_search(message.text, pieces_match, message_regex))
//        {
//            //post into that channel
//            slack::slack c{message.token.bot_token};
//            auto channel_name = pieces_match[1].str();
//            slack::user_id companion_user_id;
//            if(is_companion_installed_(c, companion_user_id))
//            {
//                c.chat.postMessage(channel_name, "Well, Statlerbot, it's time to go. Thank goodness!");
//            }
//        }
//    });
}