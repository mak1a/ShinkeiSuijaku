#include "SceneMaster.hpp"
#include <HamFramework.hpp>

/// <summary>
/// 共通データ
/// シーン間で引き継ぐ用
/// </summary>
namespace Common {
    enum class Scene {
        Title,  // タイトルシーン
        Match,  // マッチングシーン
        Game    // ゲームシーン
    };

    /// <summary>
    /// ゲームデータ
    /// </summary>
    class GameData {
    private:
        // ランダムルームのフィルター用
        ExitGames::Common::Hashtable m_hashTable;

        bool m_isMasterClient;

    public:
        GameData() : m_isMasterClient(false) {
            m_hashTable.put(L"gameType", L"photonSample");
        }

        ExitGames::Common::Hashtable& GetCustomProperties() {
            return m_hashTable;
        }

        [[nodiscard]] bool IsMasterClient() const noexcept {
            return m_isMasterClient;
        }

        void SetMasterClient(const bool isMasterClient_) {
            m_isMasterClient = isMasterClient_;
        }
    };
}

namespace EventCode::Game {
    constexpr nByte init = 1;
    constexpr nByte flip = 2;
    constexpr nByte nextTurn = 3;
}

using MyScene = Utility::SceneMaster<Common::Scene, Common::GameData>;

namespace Sample {
    class Title : public MyScene::Scene {
    private:
        s3d::RectF m_startButton;
        s3d::RectF m_exitButton;

        s3d::Transition m_startButtonTransition;
        s3d::Transition m_exitButtonTransition;

    public:
        Title(const InitData& init_)
            : IScene(init_)
            , m_startButton(s3d::Arg::center(s3d::Scene::Center()), 300, 60)
            , m_exitButton(s3d::Arg::center(s3d::Scene::Center().movedBy(0, 100)), 300, 60)
            , m_startButtonTransition(s3d::SecondsF(0.4), s3d::SecondsF(0.2))
            , m_exitButtonTransition(s3d::SecondsF(0.4), s3d::SecondsF(0.2)) {}

        void update() override {
            m_startButtonTransition.update(m_startButton.mouseOver());
            m_exitButtonTransition.update(m_exitButton.mouseOver());

            if (m_startButton.mouseOver() || m_exitButton.mouseOver()) {
                s3d::Cursor::RequestStyle(s3d::CursorStyle::Hand);
            }

            if (m_startButton.leftClicked()) {
                changeScene(Common::Scene::Match);
                return;
            }

            if (m_exitButton.leftClicked()) {
                s3d::System::Exit();
                return;
            }
        }

        void draw() const override {
            const s3d::String titleText = U"Photonサンプル";
            const s3d::Vec2 center(s3d::Scene::Center().x, 120);
            s3d::FontAsset(U"Title")(titleText).drawAt(center.movedBy(4, 6), s3d::ColorF(0.0, 0.5));
            s3d::FontAsset(U"Title")(titleText).drawAt(center);

            m_startButton.draw(s3d::ColorF(1.0, m_startButtonTransition.value())).drawFrame(2);
            m_exitButton.draw(s3d::ColorF(1.0, m_exitButtonTransition.value())).drawFrame(2);

            s3d::FontAsset(U"Menu")(U"接続する").drawAt(m_startButton.center(), s3d::ColorF(0.25));
            s3d::FontAsset(U"Menu")(U"おわる").drawAt(m_exitButton.center(), s3d::ColorF(0.25));
        }
    };

    class Match : public MyScene::Scene {
    private:
        s3d::RectF m_exitButton;

        s3d::Transition m_exitButtonTransition;

        void ConnectReturn(int errorCode, const ExitGames::Common::JString& errorString, const ExitGames::Common::JString& region, const ExitGames::Common::JString& cluster) override {
            if (errorCode) {
                s3d::Print(U"接続出来ませんでした");
                changeScene(Common::Scene::Title);  // タイトルシーンに戻る
                return;
            }

            s3d::Print(U"接続しました");
            GetClient().opJoinRandomRoom(getData().GetCustomProperties(), 2);   // 第2引数でルームに参加できる人数を設定します。
        }

        void DisconnectReturn() override {
            s3d::Print(U"切断しました");
            changeScene(Common::Scene::Title);  // タイトルシーンに戻る
        }

        void CreateRoomReturn(int localPlayerNr,
            const ExitGames::Common::Hashtable& roomProperties,
            const ExitGames::Common::Hashtable& playerProperties,
            int errorCode,
            const ExitGames::Common::JString& errorString) override {
            if (errorCode) {
                s3d::Print(U"部屋を作成出来ませんでした");
                Disconnect();
                return;
            }

            s3d::Print(U"部屋を作成しました！");
            getData().SetMasterClient(true);
        }

        void JoinRandomRoomReturn(int localPlayerNr,
            const ExitGames::Common::Hashtable& roomProperties,
            const ExitGames::Common::Hashtable& playerProperties,
            int errorCode,
            const ExitGames::Common::JString& errorString) override {
            if (errorCode) {
                s3d::Print(U"部屋が見つかりませんでした");

                CreateRoom(L"", getData().GetCustomProperties(), 2);

                s3d::Print(U"部屋を作成します...");
                return;
            }

            s3d::Print(U"部屋に接続しました!");
            getData().SetMasterClient(false);
            // この下はゲームシーンに進んだり対戦相手が設定したりする処理を書きます。
            changeScene(Common::Scene::Game);  // ゲームシーンに進む
        }

        void JoinRoomEventAction(int playerNr, const ExitGames::Common::JVector<int>& playernrs, const ExitGames::LoadBalancing::Player& player) override {
            // 部屋に入室したのが自分の場合、早期リターン
            if (GetClient().getLocalPlayer().getNumber() == player.getNumber()) {
                return;
            }

            s3d::Print(U"対戦相手が入室しました。");
            // この下はゲームシーンに進んだり設定したりする処理を書きます。
            changeScene(Common::Scene::Game);  // ゲームシーンに進む
        }

        void LeaveRoomEventAction(int playerNr, bool isInactive) override {
            s3d::Print(U"対戦相手が退室しました。");
            changeScene(Common::Scene::Title);  // タイトルシーンに戻る
        }

        /// <summary>
        /// 今回はこの関数は必要ない(何も受信しない為)
        /// </summary>
        void CustomEventAction(int playerNr, nByte eventCode, const ExitGames::Common::Object& eventContent) override {
            s3d::Print(U"受信しました");
        }

    public:
        Match(const InitData& init_)
            : IScene(init_)
            , m_exitButton(s3d::Arg::center(s3d::Scene::Center().movedBy(300, 200)), 300, 60)
            , m_exitButtonTransition(s3d::SecondsF(0.4), s3d::SecondsF(0.2)) {
            Connect();  // この関数を呼び出せば接続できる。
            s3d::Print(U"接続中...");

            GetClient().fetchServerTimestamp();
        }

        void update() override {
            m_exitButtonTransition.update(m_exitButton.mouseOver());

            if (m_exitButton.mouseOver()) {
                s3d::Cursor::RequestStyle(s3d::CursorStyle::Hand);
            }

            if (m_exitButton.leftClicked()) {
                Disconnect();   // この関数を呼び出せば切断できる。
                return;
            }
        }

        void draw() const override {
            m_exitButton.draw(s3d::ColorF(1.0, m_exitButtonTransition.value())).drawFrame(2);

            s3d::FontAsset(U"Menu")(U"タイトルに戻る").drawAt(m_exitButton.center(), s3d::ColorF(0.25));
        }
    };

    class Game : public MyScene::Scene {
    private:
        struct MyCard {
            s3d::Vec2 pos;
            double angle;
            s3d::PlayingCard::Card card;

            MyCard(const s3d::Vec2& pos_, const double angle_, const s3d::PlayingCard::Card& card_) noexcept
                : pos(pos_)
                , angle(angle_)
                , card(card_) {}
        };

        s3d::PlayingCard::Pack m_pack;
        s3d::Array<MyCard> m_cards;

        std::array<uint32, 2> m_flipNums;

        uint32 m_flipCount;
        uint32 m_score;

        enum class Turn {
            Player,
            Enemy,
        } m_turn;

        /// <summary>
        /// 送られてきたイベントの制御
        /// </summary>
        void CustomEventAction(int playerNr, nByte eventCode, const ExitGames::Common::Object& eventContent) override {
            switch (eventCode) {
            case EventCode::Game::init:
            {
                auto dic = ExitGames::Common::ValueObject<ExitGames::Common::Dictionary<nByte, int32>>(eventContent).getDataCopy();
                {
                    using namespace s3d::MathLiterals;
                    for (const auto cards = s3d::PlayingCard::CreateDeck(0, false); nByte i : s3d::step(cards.size())) {
                        m_cards[i].card = cards[*dic.getValue(i)];
                    }
                }

                break;
            }
            case EventCode::Game::flip:
            {
                auto dic = ExitGames::Common::ValueObject<ExitGames::Common::Dictionary<nByte, int32>>(eventContent).getDataCopy();
                m_cards[*dic.getValue(1)].card.flip();

                m_flipNums[m_flipCount] = m_cards[*dic.getValue(1)].card.rank;

                ++m_flipCount;
                break;
            }
            case EventCode::Game::nextTurn:
            {
                auto dic = ExitGames::Common::ValueObject<ExitGames::Common::Dictionary<nByte, bool>>(eventContent).getDataCopy();
                // めくったカードの数字が一致しているか
                if (m_flipNums[0] == m_flipNums[1]) {
                    auto itr = std::remove_if(m_cards.begin(), m_cards.end(), [](const auto x) {return x.card.isFaceSide; });

                    m_cards.erase(itr, m_cards.end());
                }
                else {
                    // 裏に戻す
                    for (auto& x : m_cards) {
                        x.card.isFaceSide = false;
                    }
                    m_turn = Turn::Player;
                }
                m_flipCount = 0;
                ++m_score;
                break;
            }
            default:
                break;
            }
        }

    public:
        Game(const InitData& init_)
            : IScene(init_)
            , m_pack(75, s3d::Palette::Red)
            , m_flipCount(0)
            , m_score(0)
            , m_turn(getData().IsMasterClient() ? Turn::Player : Turn::Enemy){
            s3d::Array<std::pair<s3d::PlayingCard::Card, int32>> cards;
            {
                auto deck = s3d::PlayingCard::CreateDeck(0, false);
                for (int32 i{}; const auto & card : deck) {
                    cards.emplace_back(card, i);
                    ++i;
                }
            }

            cards.shuffle();

            {
                using namespace s3d::MathLiterals;
                for (auto i : s3d::step(cards.size())) {
                    m_cards.emplace_back(s3d::Vec2(100 + i % 13 * (m_pack.width() * 1.2), 180 + ((i / 13)) * (m_pack.height() * 1.2))
                        , s3d::Random(-13_deg, 13_deg)
                        , cards[i].first);
                }
            }

            if (getData().IsMasterClient()) {
                ExitGames::Common::Dictionary<nByte, int32> dic;
                for (nByte i{}; const auto & [card, order] : cards) {
                    dic.put(i, order);
                    ++i;
                }
                GetClient().opRaiseEvent(true, dic, EventCode::Game::init);
            }
        }

        void update() override {
            if (!s3d::MouseL.down() || m_turn == Turn::Enemy) {
                return;
            }

            if (m_flipCount < 2) {
                // カードのクリック判定＆処理
                for (int32 i{}; auto & card : m_cards) {
                    if (m_pack.regionAt(card.pos, card.angle).leftClicked() && !card.card.isFaceSide && m_flipCount < 2) {
                        card.card.flip();

                        m_flipNums[m_flipCount] = card.card.rank;

                        ++m_flipCount;

                        ExitGames::Common::Dictionary<nByte, int32> dic;
                        dic.put(1, i);
                        GetClient().opRaiseEvent(true, dic, EventCode::Game::flip);
                        return;
                    }
                    ++i;
                }
                return;
            }

            // めくったカードの数字が一致しているか
            if (m_flipNums[0] == m_flipNums[1]) {
                auto itr = std::remove_if(m_cards.begin(), m_cards.end(), [](const auto x) {return x.card.isFaceSide;});

                m_cards.erase(itr, m_cards.end());
            }
            else {
                // 裏に戻す
                for (auto& x : m_cards) {
                    x.card.isFaceSide = false;
                }
                m_turn = Turn::Enemy;
            }
            m_flipCount = 0;
            ++m_score;

            ExitGames::Common::Dictionary<nByte, bool> dic;
            dic.put(1, true);
            GetClient().opRaiseEvent(true, dic, EventCode::Game::nextTurn);
        }

        void draw() const override {
            for (const auto& card : m_cards) 		{
                m_pack(card.card).drawAt(card.pos, card.angle);
            }
            const s3d::String turn = (m_turn == Turn::Player) ? U"あなたのターンです : " : U"相手のターンです : ";
            s3d::FontAsset(U"Menu")(turn + m_score).drawAt(s3d::Scene::CenterF().x, 40);
        }
    };
}


void Main() {
    // タイトルを設定
    s3d::Window::SetTitle(U"Photonサンプル");

    // ウィンドウの大きさを設定
    s3d::Window::Resize(1280, 720);

    // 背景色を設定
    s3d::Scene::SetBackground(s3d::ColorF(0.2, 0.8, 0.4));

    // 使用するフォントアセットを登録
    s3d::FontAsset::Register(U"Title", 120, s3d::Typeface::Heavy);
    s3d::FontAsset::Register(U"Menu", 30, s3d::Typeface::Regular);

    // シーンと遷移時の色を設定
    MyScene manager(L"/*ここにPhotonのappIDを入力してください。*/", L"1.0");

    manager.add<Sample::Title>(Common::Scene::Title)
        .add<Sample::Match>(Common::Scene::Match)
        .add<Sample::Game>(Common::Scene::Game)
        .setFadeColor(s3d::ColorF(1.0));

    //manager.init(Common::Scene::Game);

    while (s3d::System::Update()) {
        if (!manager.update()) {
            break;
        }
    }
}
