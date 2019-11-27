// pages/user_menu/user_menu.js
Page({

  /**
   * 页面的初始数据
   */
  data: {
    canIUse: wx.canIUse('button.open-type.getUserInfo'),
    userInfo:{}
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function () {
    // 查看是否授权
    wx.getSetting({
      success(res) {
        if (res.authSetting['scope.userInfo']) {
          // 已经授权，可以直接调用 getUserInfo 获取头像昵称
          wx.getUserInfo({
            success: function (res) {
              console.log(res.userInfo);
            }
          })
        }
      }
    })
  },
  now: function () {
    wx.navigateTo({
      url: '../now_order/now_order'
    })
  },

  write: function () {
    wx.navigateTo({
      url: '../write_order/write_order'
    })
  },

  check: function () {
    wx.navigateTo({
      url: '../check_order/check_order'
    })
  },
  makePhone() {
    wx.makePhoneCall({
      phoneNumber: "12234567891",
    })
  },
  navTo: function () {
    wx.navigateTo({
      url: '../user_menu/about/about'
    })
  },

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {

  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {
    
  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {

  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {

  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {

  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {

  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})