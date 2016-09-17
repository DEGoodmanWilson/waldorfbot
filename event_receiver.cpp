//
// Created by D.E. Goodman-Wilson on 8/9/16.
//

#include "event_receiver.h"
#include <slack/slack.h>
#include <random>
#include "logging.h"


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

uint8_t d100()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    auto i = dis(gen);
    return i;
}

void event_receiver::handle_error(std::string message, std::string received)
{
    // we don't have to log, because it will be logged for us.
//    LOG(ERROR) << message << " " << received;
//    std::cout << message << " " << received << std::endl;
}

void
event_receiver::handle_unknown(std::shared_ptr <slack::event::unknown> event,
                               const slack::http_event_envelope &envelope)
{
    LOG(WARNING) << "Unknown event: " << event->type;
}

void
event_receiver::handle_message(std::shared_ptr <slack::event::message> event,
                               const slack::http_event_envelope &envelope)
{
    LOG(DEBUG) << "Handling message: " << event->text;

    static std::vector <std::string> phrases = {
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


//TODO this can be highly optimized
    token_storage::token_info token;
    if (store_->get_token_for_team(envelope.team_id, token))
    {
        if (d100() <= 1) //only respond 1% of the time
        {
            auto phrase = *select_randomly(phrases.begin(), phrases.end());
            slack::slack c{token.bot_token};
            c.chat.postMessage(event->channel, phrase);
        }
    }
}

event_receiver::event_receiver(server *server, token_storage *store, const std::string &verification_token) :
        route_set{server},
        handler_{[=](const slack::team_id team_id) -> std::string
                 {
                     token_storage::token_info token;
                     if (store->get_token_for_team(team_id, token))
                     {
                         return token.bot_token;
                     }
                     return "";
                 },
                 verification_token},
        store_{store}
{

    server->handle_request(request_method::POST, "/slack/event", [&](auto req) -> response
    {
        if (req.headers.count("Bb-Slackaccesstoken"))
        {
            token_storage::token_info token{
                    req.headers["Bb-Slackaccesstoken"],
                    req.headers["Bb-Slackbotaccesstoken"],
                    req.headers["Bb-Slackuserid"],
                    req.headers["Bb-Slackbotuserid"],
            };
            store_->set_token(req.headers["Bb-Slackteamid"], token);
        }

        if (!req.body.empty())
        {
            return {handler_.handle_event(req.body)};
        }
        else if (!req.params["event"].empty())
        {
            return {handler_.handle_event(req.params["event"])};
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

    //dialog responses
    handler_.hears(std::regex{"^I wonder if there really is life on another planet.$"}, [](const auto &message)
    {
        message.reply("Why do you care? You don’t have a life on this one?");
    });

    handler_.hears(std::regex{"^Waldorf, the bunny ran away!$"}, [](const auto &message)
    {
        message.reply("Well, you know what that makes him…");
        message.reply("Smarter than us");
    });

    handler_.hears(std::regex{"^Boo!$"}, [](const auto &message)
    {
        message.reply("Boooo!");
    });
    handler_.hears(std::regex{"^That was the worst thing I’ve ever heard!$"}, [](const auto &message)
    {
        message.reply("It was terrible!");
    });
    handler_.hears(std::regex{"^Horrendous!$"}, [](const auto &message)
    {
        message.reply("Well it wasn’t that bad.");
    });
    handler_.hears(std::regex{"^Oh, yeah\?$"}, [](const auto &message)
    {
        message.reply("Well, there were parts of it I liked!");
    });
    handler_.hears(std::regex{"^Well, I liked alot of it.$"}, [](const auto &message)
    {
        message.reply("Yeah, it was GOOD actually.");
    });
    handler_.hears(std::regex{"^It was great!$"}, [](const auto &message)
    {
        message.reply("It was wonderful!");
    });
    handler_.hears(std::regex{"^Yeah, bravo!$"}, [](const auto &message)
    {
        message.reply("More!");
    });

    handler_.hears(std::regex{"^Hm. Do you think this channel is educational\?$"}, [](const auto &message)
    {
        message.reply("Yes. It'll drive people to read books.");
    });

    handler_.hears(std::regex{"^He was doing okay until he left the channel.$"}, [](const auto &message)
    {
        message.reply("Wrong. He was doing okay until he _joined_ the channel.");
    });

    handler_.hears(std::regex{"^I liked that last message.$"}, [](const auto &message)
    {
        message.reply("What did you like about it?");
    });

    handler_.hears(std::regex{"^Why is that\?$"}, [](const auto &message)
    {
        message.reply("I forgot.");
    });

    handler_.hears(std::regex{"^I'm going to see my lawyer!$"}, [](const auto &message)
    {
        message.reply("Why?");
    });

    handler_.hears(std::regex{"^You gave him a one\?$"}, [](const auto &message)
    {
        message.reply("He's never been better.");
    });

    handler_.hears(std::regex{"^You know, the older I get, the more I appreciate good wit.$"}, [](const auto &message)
    {
        message.reply("Yeah? What's that got to do with what we just read?");
    });

    handler_.hears(std::regex{"^That really offended me. I'm a student of Shakespeare.$"}, [](const auto &message)
    {
        message.reply("Ha! You were a student _with_ Shakespeare.");
    });

    handler_.hears(std::regex{"^I love it! I love it!$"}, [](const auto &message)
    {
        message.reply("Of course he loves it; he's the kind of guy who plants poison ivy.");
    });

    handler_.hears(std::regex{"^More! More!$"}, [](const auto &message)
    {
        message.reply("No, not so loud! They may hear you!");
    });

    handler_.hears(std::regex{"^You plan to like this channel\?$"}, [](const auto &message)
    {
        message.reply(":tv: No, I plan to watch television!");
    });

    handler_.hears(std::regex{"^\"Beach Blanket Frankenstein\".$"}, [](const auto &message)
    {
        message.reply("Awful.");
    });
    handler_.hears(std::regex{"^Terrible film!$"}, [](const auto &message)
    {
        message.reply("Yeah, well, we could read this channel instead.");
    });
    handler_.hears(std::regex{"^:eyes:$"}, [](const auto &message)
    {
        message.reply(":eyes:");
    });
    handler_.hears(std::regex{"^Wonderful.$"}, [](const auto &message)
    {
        message.reply("Terrific film!");
    });

    handler_.hears(std::regex{"^How do _we read_ it\?$"}, [](const auto &message)
    {
        message.reply("_Why_ do we read it?");
    });

    handler_.hears(std::regex{"^I don't believe it! They've managed the impossible! What an achievement! Bravo, bravo!$"},
                   [](const auto &message)
                   {
                       message.reply("What, you mean you actually like this channel now?");
                   });

    handler_.hears(std::regex{"^Well, what ails ya\?$"}, [](const auto &message)
    {
        message.reply("Insomnia.");
    });

    handler_.hears(std::regex{"^Did you like it\?$"}, [](const auto &message)
    {
        message.reply("No.");
    });

    handler_.hears(std::regex{"^I wonder if anybody reads this channel besides us\?$"}, [](const auto &message)
    {
        message.reply(":zzz:");
    });

    handler_.hears(std::regex{"^What's wrong with you\?$"}, [](const auto &message)
    {
        message.reply("It's either this channel or indigestion. I hope it's indigestion.");
    });
    handler_.hears(std::regex{"^Why indigestion\?$"}, [](const auto &message)
    {
        message.reply("It'll get better in a little while.");
    });

    handler_.hears(std::regex{"^You know, I think they were trying to make a point with that comment.$"}, [](const auto &message)
    {
        message.reply("What's the point?");
    });

    handler_.hears(std::regex{"^You know, that was almost funny.$"}, [](const auto &message)
    {
        message.reply("They better be careful, they'll spoil a perfect record.");
    });

    handler_.hears(std::regex{"^Are you ready for the end of the world\?$"}, [](const auto &message)
    {
        message.reply("Sure, it couldn't be worse than this channel.");
    });
}